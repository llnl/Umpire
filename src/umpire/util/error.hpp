//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-21, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#ifndef UMPIRE_runtime_error_HPP
#define UMPIRE_runtime_error_HPP

#include <stdexcept>
#include <string>

#include "fmt/format.h"
#include "umpire/util/Macros.hpp"

namespace umpire {

class Allocator;

class runtime_error : public std::runtime_error {
 public:
  runtime_error(const std::string& msg, const std::string& file, int line)
      : std::runtime_error(msg), m_message(msg), m_file(file), m_line(line)
  {
    m_what = this->message();
  }

  virtual ~runtime_error() = default;

  std::string message() const
  {
    umpire::util::backtrace bt;
    umpire::util::backtracer<umpire::util::trace_always>::get_backtrace(bt);
    std::stringstream oss;
    oss << "! Umpire runtime_error [" << m_file << ":" << m_line << "]: ";
    oss << m_message;
    oss << std::endl << umpire::util::backtracer<umpire::util::trace_always>::print(bt) << std::endl;
    return oss.str();
  }

  virtual const char* what() const throw()
  {
    return m_what.c_str();
  }

 private:
  std::string m_message;
  std::string m_file;
  std::string m_what;
  int m_line;
};

class out_of_memory_error : public umpire::runtime_error {
 public:
  out_of_memory_error(const std::string& msg, const std::string& file, int line) : runtime_error(msg, file, line)
  {
  }

  std::size_t requested_size()
  {
    return m_requested;
  }

  int get_allocator_id()
  {
    return m_allocator;
  }

 private:
  void set_allocator_id(int id)
  {
    m_allocator = id;
  }

  void set_requested_size(std::size_t s)
  {
    m_requested = s;
  }

  std::size_t m_requested{0};
  int m_allocator{-1};

  friend class Allocator;
};

class unknown_pointer_error : public umpire::runtime_error {
 public:
  unknown_pointer_error(const std::string& msg, const std::string& file, int line) : runtime_error(msg, file, line)
  {
  }

  inline void* get_pointer() const
  {
    return m_pointer;
  }

 private:
  void set_pointer(void* p)
  {
    m_pointer = p;
  }

  void* m_pointer;

  friend class Allocator;
};

class resource_error : public umpire::runtime_error {
 public:
  resource_error(const std::string& msg, const std::string& file, int line) : runtime_error(msg, file, line)
  {
  }
};

} // end of namespace umpire

#if defined(__CUDA_ARCH__)
#define UMPIRE_ERROR(type, msg) asm("trap;");
#elif defined(__HIP_DEVICE_COMPILE__)
#define UMPIRE_ERROR(type, msg) abort();
#else
#define UMPIRE_ERROR(type, msg)                                           \
  {                                                                       \
    type e{msg, std::string{__FILE__}, __LINE__};                         \
    UMPIRE_LOG(Error, e.what());                                          \
    throw e;                                                              \
  }
#endif

#endif // UMPIRE_runtime_error_HPP
