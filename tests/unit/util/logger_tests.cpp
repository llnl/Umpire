//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#include "gtest/gtest.h"
#include "umpire/config.hpp"

#if defined(UMPIRE_ENABLE_LOGGING)
#include <cstdlib>
#include <fstream>
#include <string>

#include "umpire/util/Logger.hpp"
#include "umpire/util/io.hpp"

#if !defined(_MSC_VER)
#include <unistd.h>
#else
#include <process.h>
#define getpid _getpid
#define unsetenv(name) _putenv_s(name, "")
#endif

class LoggerTest : public ::testing::Test {
 protected:
  void SetUp() override
  {
    // Clear environment variables and logger state before each test so
    // every test exercises a fresh Logger::initialize()
#if defined(_MSC_VER)
    _putenv_s("UMPIRE_LOG_LEVEL", "");
    _putenv_s("UMPIRE_LOG_TO_CONSOLE", "");
    _putenv_s("UMPIRE_LOG_ASYNC", "");
    _putenv_s("UMPIRE_LOG_QUEUE_SIZE", "");
#else
    unsetenv("UMPIRE_LOG_LEVEL");
    unsetenv("UMPIRE_LOG_TO_CONSOLE");
    unsetenv("UMPIRE_LOG_ASYNC");
    unsetenv("UMPIRE_LOG_QUEUE_SIZE");
#endif
    umpire::util::Logger::reset();
  }

  void TearDown() override
  {
    // Clean up environment and logger state after each test
    SetUp();
  }

  std::string getLogFilename()
  {
    const std::string& output_dir = umpire::util::get_io_output_dir();
    const std::string& basename = umpire::util::get_io_output_basename();
    const int pid = getpid();

    // Find the log file (there may be multiple with different IDs)
    std::string log_file;
    for (int id = 0; id < 100; ++id) {
      std::string candidate =
          output_dir + "/" + basename + "." + std::to_string(pid) + "." + std::to_string(id) + ".log";
      std::ifstream test_file(candidate);
      if (test_file.good()) {
        log_file = candidate;
        // Continue to find the most recent one
      }
    }
    return log_file;
  }

  bool logFileContains(const std::string& text)
  {
    std::string log_file = getLogFilename();
    if (log_file.empty()) {
      return false;
    }

    std::ifstream file(log_file);
    if (!file.is_open()) {
      return false;
    }

    std::string line;
    while (std::getline(file, line)) {
      if (line.find(text) != std::string::npos) {
        return true;
      }
    }
    return false;
  }
};

TEST_F(LoggerTest, InitializeWithoutEnvVariable)
{
  // Without UMPIRE_LOG_LEVEL set, logging should be disabled
  umpire::util::Logger::initialize();

  ASSERT_FALSE(umpire::util::Logger::shouldLog(umpire::util::message::Info));
  ASSERT_FALSE(umpire::util::Logger::shouldLog(umpire::util::message::Debug));
  ASSERT_FALSE(umpire::util::Logger::shouldLog(umpire::util::message::Warning));
  ASSERT_FALSE(umpire::util::Logger::shouldLog(umpire::util::message::Error));

  umpire::util::Logger::finalize();
}

TEST_F(LoggerTest, InitializeWithLogLevel)
{
#if defined(_MSC_VER)
  _putenv_s("UMPIRE_LOG_LEVEL", "INFO");
#else
  setenv("UMPIRE_LOG_LEVEL", "INFO", 1);
#endif

  umpire::util::Logger::initialize();

  ASSERT_TRUE(umpire::util::Logger::shouldLog(umpire::util::message::Info));
  ASSERT_TRUE(umpire::util::Logger::shouldLog(umpire::util::message::Warning));
  ASSERT_TRUE(umpire::util::Logger::shouldLog(umpire::util::message::Error));
  ASSERT_FALSE(umpire::util::Logger::shouldLog(umpire::util::message::Debug));

  umpire::util::Logger::finalize();
}

TEST_F(LoggerTest, LogLevelDebug)
{
#if defined(_MSC_VER)
  _putenv_s("UMPIRE_LOG_LEVEL", "DEBUG");
#else
  setenv("UMPIRE_LOG_LEVEL", "DEBUG", 1);
#endif

  umpire::util::Logger::initialize();

  ASSERT_TRUE(umpire::util::Logger::shouldLog(umpire::util::message::Debug));
  ASSERT_TRUE(umpire::util::Logger::shouldLog(umpire::util::message::Info));
  ASSERT_TRUE(umpire::util::Logger::shouldLog(umpire::util::message::Warning));
  ASSERT_TRUE(umpire::util::Logger::shouldLog(umpire::util::message::Error));

  umpire::util::Logger::finalize();
}

TEST_F(LoggerTest, LogLevelError)
{
#if defined(_MSC_VER)
  _putenv_s("UMPIRE_LOG_LEVEL", "ERROR");
#else
  setenv("UMPIRE_LOG_LEVEL", "ERROR", 1);
#endif

  umpire::util::Logger::initialize();

  ASSERT_TRUE(umpire::util::Logger::shouldLog(umpire::util::message::Error));
  ASSERT_FALSE(umpire::util::Logger::shouldLog(umpire::util::message::Warning));
  ASSERT_FALSE(umpire::util::Logger::shouldLog(umpire::util::message::Info));
  ASSERT_FALSE(umpire::util::Logger::shouldLog(umpire::util::message::Debug));

  umpire::util::Logger::finalize();
}

TEST_F(LoggerTest, CaseInsensitiveLogLevel)
{
#if defined(_MSC_VER)
  _putenv_s("UMPIRE_LOG_LEVEL", "info");
#else
  setenv("UMPIRE_LOG_LEVEL", "info", 1);
#endif

  umpire::util::Logger::initialize();

  ASSERT_TRUE(umpire::util::Logger::shouldLog(umpire::util::message::Info));

  umpire::util::Logger::finalize();
}

TEST_F(LoggerTest, LogMessageWrittenToFile)
{
#if defined(_MSC_VER)
  _putenv_s("UMPIRE_LOG_LEVEL", "INFO");
  _putenv_s("UMPIRE_LOG_TO_CONSOLE", "off");
#else
  setenv("UMPIRE_LOG_LEVEL", "INFO", 1);
  setenv("UMPIRE_LOG_TO_CONSOLE", "off", 1);
#endif

  umpire::util::Logger::initialize();

  const std::string test_message = "LoggerTest_UniqueMessage_12345";
  umpire::util::Logger::log(umpire::util::message::Info, test_message, __FILE__, __LINE__);

  // reset() flushes and destroys the logger, guaranteeing the message is on disk
  umpire::util::Logger::reset();

  // Check that the message was written to the log file
  ASSERT_TRUE(logFileContains(test_message));
}

TEST_F(LoggerTest, AsyncLoggingMode)
{
#if defined(_MSC_VER)
  _putenv_s("UMPIRE_LOG_LEVEL", "INFO");
  _putenv_s("UMPIRE_LOG_ASYNC", "on");
  _putenv_s("UMPIRE_LOG_QUEUE_SIZE", "4096");
#else
  setenv("UMPIRE_LOG_LEVEL", "INFO", 1);
  setenv("UMPIRE_LOG_ASYNC", "on", 1);
  setenv("UMPIRE_LOG_QUEUE_SIZE", "4096", 1);
#endif

  umpire::util::Logger::initialize();

  const std::string test_message = "AsyncTest_Message_67890";
  umpire::util::Logger::log(umpire::util::message::Info, test_message, __FILE__, __LINE__);

  // reset() joins the async worker thread, guaranteeing the message is on disk
  // (finalize() alone only enqueues a flush request in async mode)
  umpire::util::Logger::reset();

  // Message should still be written even in async mode
  ASSERT_TRUE(logFileContains(test_message));
}

TEST_F(LoggerTest, InvalidQueueSizeDefaultsToMinimum)
{
#if defined(_MSC_VER)
  _putenv_s("UMPIRE_LOG_LEVEL", "INFO");
  _putenv_s("UMPIRE_LOG_ASYNC", "on");
  _putenv_s("UMPIRE_LOG_QUEUE_SIZE", "100"); // Too small, should use minimum (1024)
#else
  setenv("UMPIRE_LOG_LEVEL", "INFO", 1);
  setenv("UMPIRE_LOG_ASYNC", "on", 1);
  setenv("UMPIRE_LOG_QUEUE_SIZE", "100", 1); // Too small
#endif

  // Should not crash even with invalid queue size
  ASSERT_NO_THROW(umpire::util::Logger::initialize());

  umpire::util::Logger::finalize();
}

TEST_F(LoggerTest, NegativeQueueSizeUsesDefault)
{
#if defined(_MSC_VER)
  _putenv_s("UMPIRE_LOG_LEVEL", "INFO");
  _putenv_s("UMPIRE_LOG_ASYNC", "on");
  _putenv_s("UMPIRE_LOG_QUEUE_SIZE", "-500"); // Negative, should use default (8192)
#else
  setenv("UMPIRE_LOG_LEVEL", "INFO", 1);
  setenv("UMPIRE_LOG_ASYNC", "on", 1);
  setenv("UMPIRE_LOG_QUEUE_SIZE", "-500", 1);
#endif

  // Should not crash
  ASSERT_NO_THROW(umpire::util::Logger::initialize());

  umpire::util::Logger::finalize();
}

TEST_F(LoggerTest, MultipleInitializeCalls)
{
#if defined(_MSC_VER)
  _putenv_s("UMPIRE_LOG_LEVEL", "INFO");
#else
  setenv("UMPIRE_LOG_LEVEL", "INFO", 1);
#endif

  // Multiple initialize calls should be safe
  umpire::util::Logger::initialize();
  umpire::util::Logger::initialize();
  umpire::util::Logger::initialize();

  ASSERT_TRUE(umpire::util::Logger::shouldLog(umpire::util::message::Info));

  umpire::util::Logger::finalize();
}

TEST_F(LoggerTest, ConsoleOutputDisabled)
{
#if defined(_MSC_VER)
  _putenv_s("UMPIRE_LOG_LEVEL", "INFO");
  _putenv_s("UMPIRE_LOG_TO_CONSOLE", "0");
#else
  setenv("UMPIRE_LOG_LEVEL", "INFO", 1);
  setenv("UMPIRE_LOG_TO_CONSOLE", "0", 1);
#endif

  // Should initialize successfully with console disabled
  ASSERT_NO_THROW(umpire::util::Logger::initialize());

  umpire::util::Logger::finalize();
}

TEST_F(LoggerTest, ConsoleOutputEnabled)
{
#if defined(_MSC_VER)
  _putenv_s("UMPIRE_LOG_LEVEL", "INFO");
  _putenv_s("UMPIRE_LOG_TO_CONSOLE", "true");
#else
  setenv("UMPIRE_LOG_LEVEL", "INFO", 1);
  setenv("UMPIRE_LOG_TO_CONSOLE", "true", 1);
#endif

  // Should initialize successfully with console enabled
  ASSERT_NO_THROW(umpire::util::Logger::initialize());

  umpire::util::Logger::finalize();
}

TEST_F(LoggerTest, DefaultConsoleOutputDisabled)
{
#if defined(_MSC_VER)
  _putenv_s("UMPIRE_LOG_LEVEL", "INFO");
  // Don't set UMPIRE_LOG_TO_CONSOLE - console defaults to disabled (file only)
#else
  setenv("UMPIRE_LOG_LEVEL", "INFO", 1);
  // Don't set UMPIRE_LOG_TO_CONSOLE - console defaults to disabled (file only)
#endif

  // Should initialize successfully with default console output (disabled);
  // the behavioral stderr check lives in tests/integration/io/log_tests_runner.py
  ASSERT_NO_THROW(umpire::util::Logger::initialize());

  umpire::util::Logger::finalize();
}

#endif // UMPIRE_ENABLE_LOGGING

int main(int argc, char** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
