//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#ifndef UMPIRE_Macros_HPP
#define UMPIRE_Macros_HPP

#include <cassert>
#include <iostream>
#include <mutex>
#include <sstream>

#include "umpire/config.hpp"
#include "umpire/util/backtrace.hpp"
#include "umpire/util/io.hpp"

#define UMPIRE_ASSERT(condition) assert(condition)

#ifdef UMPIRE_ENABLE_LOGGING

#include "umpire/util/Logger.hpp"

// __FILE__ is passed as a const char* (static storage duration) rather than
// a std::string: asynchronous logging reads the filename from a background
// thread after this macro has returned.
#define UMPIRE_LOG(lvl, msg)                                                                              \
  do {                                                                                                    \
    if (umpire::util::Logger::shouldLog(umpire::util::message::lvl)) {                                    \
      std::ostringstream umpire_log_stream;                                                               \
      umpire_log_stream << " " << __func__ << " " << msg;                                                 \
      umpire::util::Logger::log(umpire::util::message::lvl, umpire_log_stream.str(), __FILE__, __LINE__); \
    }                                                                                                     \
  } while (0)

#else

#define UMPIRE_LOG(lvl, msg) ((void)0)

#endif // UMPIRE_ENABLE_LOGGING

#define UMPIRE_UNUSED_ARG(x)

#define UMPIRE_USE_VAR(x) static_cast<void>(x)

#if defined(UMPIRE_ENABLE_BACKTRACE)
#define UMPIRE_RECORD_BACKTRACE(record) \
  umpire::util::backtracer<umpire::util::trace_optional>::get_backtrace(record.allocation_backtrace)
#else
#define UMPIRE_RECORD_BACKTRACE(backtrace) ((void)0)
#endif

#if (__cplusplus >= 201402L)
#if defined(__has_cpp_attribute)
#if __has_cpp_attribute(deprecated)
#define UMPIRE_HAS_CXX_ATTRIBUTE_DEPRECATED 1
#endif
#endif
#endif

#if defined(UMPIRE_HAS_CXX_ATTRIBUTE_DEPRECATED)
#define UMPIRE_DEPRECATE(Msg) [[deprecated(Msg)]]
#define UMPIRE_DEPRECATE_ALIAS(Msg) [[deprecated(Msg)]]

#elif defined(_MSC_VER)

// for MSVC, use __declspec
#define UMPIRE_DEPRECATE(Msg) __declspec(deprecated(Msg))
#define UMPIRE_DEPRECATE_ALIAS(Msg)

#else

// else use __attribute__(deprecated("Message"))
#define UMPIRE_DEPRECATE(Msg) __attribute__((deprecated(Msg)))
#define UMPIRE_DEPRECATE_ALIAS(Msg)

#endif

#if 0
#define UMPIRE_INTERNAL_TRACK(p, s) registerAllocation(p, s, this);
#define UMPIRE_INTERNAL_UNTRACK(p) deregisterAllocation(p, this);
#else
#define UMPIRE_INTERNAL_TRACK(p, s)
#define UMPIRE_INTERNAL_UNTRACK(p) umpire::util::AllocationRecord{};
#endif

#endif // UMPIRE_Macros_HPP
