//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#pragma once

#include <cstddef>
#include <string>

#include "camp/resource.hpp"
#include "umpire/util/error.hpp"

namespace umpire {
namespace op {

template <typename Src, typename Dst>
struct copy {};

template <typename Src>
struct memset {};

template <typename Src>
struct device_memset {};

template <typename Platform>
struct reallocate {
  template <typename T>
  static T* exec(T** ptr, std::size_t new_size);

  template <typename T>
  static camp::resources::EventProxy<camp::resources::Resource> exec(T** ptr_ptr, std::size_t new_size,
                                                                     camp::resources::Resource& ctx);

  static void* exec(void** ptr_ptr, std::size_t new_size);

  static camp::resources::EventProxy<camp::resources::Resource> exec(void** ptr_ptr, std::size_t new_size,
                                                                     camp::resources::Resource& ctx);

 private:
  template <typename PtrType>
  static PtrType* reallocate_impl_sync(PtrType** ptr_ptr, std::size_t new_size);

  template <typename PtrType>
  static camp::resources::EventProxy<camp::resources::Resource> reallocate_impl_async(PtrType** ptr_ptr,
                                                                                      std::size_t new_size,
                                                                                      camp::resources::Resource& ctx);
};

#define DEFINE_ADVICE_OP(op_name, name_str)                                                                       \
  template <typename Src>                                                                                         \
  struct op_name {                                                                                                \
    static void exec(void*, int, std::size_t)                                                                     \
    {                                                                                                             \
      UMPIRE_ERROR(runtime_error, std::string{"Memory advice "} + name_str + " not supported for this platform"); \
    }                                                                                                             \
                                                                                                                  \
    static camp::resources::EventProxy<camp::resources::Resource> exec(void*, int, std::size_t,                   \
                                                                       camp::resources::Resource&)                \
    {                                                                                                             \
      UMPIRE_ERROR(runtime_error, std::string{"Memory advice "} + name_str + " not supported for this platform"); \
    }                                                                                                             \
  };

DEFINE_ADVICE_OP(set_accessed_by, "SET_ACCESSED_BY")
DEFINE_ADVICE_OP(unset_accessed_by, "UNSET_ACCESSED_BY")
DEFINE_ADVICE_OP(set_preferred_location, "SET_PREFERRED_LOCATION")
DEFINE_ADVICE_OP(unset_preferred_location, "UNSET_PREFERRED_LOCATION")
DEFINE_ADVICE_OP(set_read_mostly, "SET_READ_MOSTLY")
DEFINE_ADVICE_OP(unset_read_mostly, "UNSET_READ_MOSTLY")
DEFINE_ADVICE_OP(set_coarse_grain, "SET_COARSE_GRAIN")
DEFINE_ADVICE_OP(unset_coarse_grain, "UNSET_COARSE_GRAIN")
DEFINE_ADVICE_OP(prefetch, "PREFETCH")

#undef DEFINE_ADVICE_OP

} // namespace op
} // namespace umpire
