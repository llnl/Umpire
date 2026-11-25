//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#include "umpire/util/Logger.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <mutex>
#include <vector>

#include "spdlog/async.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/null_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"
#include "umpire/util/MPI.hpp"
#include "umpire/util/io.hpp"

#if !defined(_MSC_VER)
#include <unistd.h>
#else
#include <process.h>
#define getpid _getpid
#endif

namespace umpire {
namespace util {

message::Level Logger::s_level = message::Info;
bool Logger::s_enabled = false;
bool Logger::s_initialized = false;

namespace {

// The logger is held through an intentionally-leaked shared_ptr so that its
// destructor never runs. ResourceManager may be destroyed during static
// destruction (e.g. tests that bind it to a static reference), and the
// relative destruction order of statics across translation units is
// unspecified: a static shared_ptr member here could be destroyed before the
// ResourceManager destructor calls finalize(), leaving finalize() to read
// freed memory. Leaking the holder keeps it valid for the whole program.
std::shared_ptr<spdlog::logger>& logger_holder()
{
  static auto* holder = new std::shared_ptr<spdlog::logger>{};
  return *holder;
}

// Guards initialize()/reset()/finalize(). shouldLog() and log() are
// deliberately lock-free: they only read s_initialized/s_enabled/s_level,
// and callers are expected to initialize the logger before logging
// concurrently.
std::mutex& logger_mutex()
{
  static auto* mutex = new std::mutex{};
  return *mutex;
}

// Whether the current logger was created in async mode; reset() must shut
// down spdlog's thread pool in that case to durably flush queued messages.
bool s_used_async{false};

const char* MessageLevelName[message::Num_Levels] = {"ERROR", "WARNING", "INFO", "DEBUG"};

bool case_insensitive_match(const std::string& s1, const std::string& s2)
{
  return (s1.size() == s2.size()) && std::equal(s1.begin(), s1.end(), s2.begin(),
                                                [](char c1, char c2) { return std::toupper(c1) == std::toupper(c2); });
}

} // namespace

message::Level Logger::parseEnvLogLevel() noexcept
{
  const char* env_level = std::getenv("UMPIRE_LOG_LEVEL");
  if (!env_level) {
    return message::Info; // default
  }

  std::string level_str(env_level);
  for (int i = 0; i < message::Num_Levels; ++i) {
    if (case_insensitive_match(level_str, MessageLevelName[i])) {
      return static_cast<message::Level>(i);
    }
  }

  return message::Info; // fallback
}

std::string Logger::generateLogFilename()
{
  const std::string& output_dir = get_io_output_dir();
  const std::string& basename = get_io_output_basename();
  const int pid = getpid();

  return make_unique_filename(output_dir, basename, pid, "log");
}

spdlog::level::level_enum Logger::convertLevel(message::Level level) noexcept
{
  switch (level) {
    case message::Error:
      return spdlog::level::err;
    case message::Warning:
      return spdlog::level::warn;
    case message::Info:
      return spdlog::level::info;
    case message::Debug:
      return spdlog::level::debug;
    default:
      return spdlog::level::info;
  }
}

void Logger::initialize()
{
  std::lock_guard<std::mutex> lock{logger_mutex()};

  if (s_initialized) {
    return;
  }

  // Logging is enabled only when UMPIRE_LOG_LEVEL is set
  const char* env_enable_log = std::getenv("UMPIRE_LOG_LEVEL");
  if (!env_enable_log) {
    // Logging disabled: install a null logger so getSpdlogger() is always
    // safe to call, but keep s_enabled false so shouldLog() lets UMPIRE_LOG
    // skip message formatting entirely.
    // Don't use spdlog::register_logger to avoid static initialization order issues
    auto null_sink = std::make_shared<spdlog::sinks::null_sink_mt>();
    logger_holder() = std::make_shared<spdlog::logger>("umpire", null_sink);
    s_enabled = false;
    s_initialized = true;
    return;
  }

  s_level = parseEnvLogLevel();

  // Check for async mode
  const char* env_async = std::getenv("UMPIRE_LOG_ASYNC");
  const bool use_async = (env_async && (std::string(env_async) == "1" || case_insensitive_match(env_async, "true") ||
                                        case_insensitive_match(env_async, "on")));

  // Get queue size for async logging (default 8192, minimum 1024)
  const char* env_queue_size = std::getenv("UMPIRE_LOG_QUEUE_SIZE");
  size_t queue_size = 8192;
  if (env_queue_size) {
    const int parsed_size = std::atoi(env_queue_size);
    // Validate: must be positive and at least 1024 to avoid performance issues
    queue_size = (parsed_size > 0) ? std::max(1024, parsed_size) : 8192;
  }

  // Initialize async thread pool if needed
  if (use_async && !spdlog::thread_pool()) {
    spdlog::init_thread_pool(queue_size, 1);
  }
  s_used_async = use_async;

  // Create the output directory if needed. Under MPI this is coordinated so
  // only rank 0 creates it, with a barrier before any rank opens its file.
  make_io_dir(get_io_output_dir());

  // Create sinks
  std::vector<spdlog::sink_ptr> sinks;

  // File sink (always enabled when UMPIRE_LOG_LEVEL is set)
  std::string log_filename = generateLogFilename();
  auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_filename, false);
  sinks.push_back(file_sink);

  // Console sink: disabled by default, matching the historical behavior
  // where log messages went to the log file only. Users can opt in with
  // UMPIRE_LOG_TO_CONSOLE=1/true/on.
  const char* env_console = std::getenv("UMPIRE_LOG_TO_CONSOLE");
  const bool log_to_console =
      (env_console && (std::string(env_console) == "1" || case_insensitive_match(env_console, "true") ||
                       case_insensitive_match(env_console, "on")));
  if (log_to_console) {
    auto console_sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
    sinks.push_back(console_sink);
  }

  // Create logger (sync or async)
  // Don't use spdlog::register_logger to avoid static initialization order issues
  if (use_async) {
    logger_holder() = std::make_shared<spdlog::async_logger>(
        "umpire", sinks.begin(), sinks.end(), spdlog::thread_pool(), spdlog::async_overflow_policy::block);
  } else {
    logger_holder() = std::make_shared<spdlog::logger>("umpire", sinks.begin(), sinks.end());
  }

  // Set format pattern to match previous output: [LEVEL][file:line]: message
  logger_holder()->set_pattern("[%^%L%$][%s:%#]: %v");

  logger_holder()->set_level(convertLevel(s_level));

  // Flush on every message for Error level (safety)
  logger_holder()->flush_on(spdlog::level::err);

  s_enabled = true;
  s_initialized = true;

  MPI::logMpiInfo();
}

void Logger::finalize()
{
  std::lock_guard<std::mutex> lock{logger_mutex()};

  // Flush, but never destroy, the logger. The intentionally-leaked holder
  // (see logger_holder()) guarantees the logger itself is still alive here
  // even during static destruction, so flushing is safe. We avoid tearing
  // the logger down because other statics may still log afterwards, and
  // destroying spdlog state during static destruction is not worth the
  // ordering headaches outside of tests (see reset()).
  if (logger_holder()) {
    try {
      logger_holder()->flush();
    } catch (...) {
      // Ignore flush errors during shutdown (e.g. the log file is gone)
    }
  }
}

void Logger::reset()
{
  std::lock_guard<std::mutex> lock{logger_mutex()};

  if (logger_holder()) {
    try {
      logger_holder()->flush();
      if (s_used_async) {
        // Join spdlog's worker thread so queued messages are durably
        // written before the logger is destroyed. Async flush() only
        // enqueues a flush request, so this is required for tests that
        // read the log file immediately after reset().
        logger_holder().reset();
        spdlog::shutdown();
      }
    } catch (...) {
      // Ignore errors: reset() must always leave the logger reinitializable
    }
  }

  logger_holder().reset();
  s_used_async = false;
  s_enabled = false;
  s_initialized = false;
  s_level = message::Info;
}

bool Logger::shouldLog(message::Level level) noexcept
{
  return s_initialized && s_enabled && (level <= s_level);
}

void Logger::log(message::Level level, const std::string& message, const char* fileName, int line) noexcept
{
  if (!shouldLog(level) || !logger_holder()) {
    return;
  }

  // fileName must have static storage duration (e.g. __FILE__): in async
  // mode spdlog copies the message payload but not the source_loc filename,
  // so the pointer is dereferenced later from the worker thread.
  logger_holder()->log(spdlog::source_loc{fileName, line, ""}, convertLevel(level), message);
}

std::shared_ptr<spdlog::logger> Logger::getSpdlogger()
{
  return logger_holder();
}

} // end namespace util
} // end namespace umpire
