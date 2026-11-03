//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#ifndef UMPIRE_Logger_HPP
#define UMPIRE_Logger_HPP

#include <memory>
#include <string>

// Forward declare spdlog types to avoid including spdlog.h in this header.
// NOTE: the fixed underlying type (": int") must match spdlog's own
// declaration of level_enum exactly (see spdlog/common.h) or the program is
// ill-formed. Re-check this when updating the spdlog submodule.
namespace spdlog {
class logger;
namespace level {
enum level_enum : int;
}
} // namespace spdlog

namespace umpire {
namespace util {

namespace message {
enum Level { Error, Warning, Info, Debug, Num_Levels };
} // end namespace message

class Logger {
 public:
  /*!
   * \brief Initialize the global logger.
   *
   * Called once from the ResourceManager constructor. Subsequent calls are
   * no-ops until reset() is called. Logging is enabled only when the
   * UMPIRE_LOG_LEVEL environment variable is set.
   */
  static void initialize();

  /*!
   * \brief Flush any buffered log messages.
   *
   * Called from the ResourceManager destructor. The logger is flushed but
   * never destroyed (see the implementation for the static destruction
   * order rationale).
   */
  static void finalize();

  /*!
   * \brief Flush and destroy the logger so initialize() can run again.
   *
   * Intended for testing only: joins the async logging thread (if any) so
   * queued messages are durably written, then clears all logger state.
   */
  static void reset();

  /*!
   * \brief Check if a message at the given level would be logged.
   *
   * Used by the UMPIRE_LOG macro to skip message formatting entirely when
   * logging is disabled or the level is filtered out.
   *
   * \param level Level of the message
   *
   * \return true if a message at this level would be logged
   */
  static bool shouldLog(message::Level level) noexcept;

  /*!
   * \brief Log a message at the specified level.
   *
   * \param level Level of the message
   * \param message The message to log
   * \param fileName Source file of the message. Must point to storage that
   *        outlives the logger (e.g. the __FILE__ string literal), since
   *        asynchronous logging reads it from a background thread.
   * \param line Source line of the message
   */
  static void log(message::Level level, const std::string& message, const char* fileName, int line) noexcept;

  /*!
   * \brief Get the underlying spdlog logger (for advanced usage).
   *
   * \return The spdlog logger, or nullptr if initialize() has not been
   *         called.
   */
  static std::shared_ptr<spdlog::logger> getSpdlogger();

 private:
  Logger() = delete;
  ~Logger() = delete;

  static spdlog::level::level_enum convertLevel(message::Level level) noexcept;
  static message::Level parseEnvLogLevel() noexcept;
  static std::string generateLogFilename();

  static message::Level s_level;
  static bool s_enabled;
  static bool s_initialized;
};

} // end namespace util
} // end namespace umpire

#endif // UMPIRE_Logger_HPP
