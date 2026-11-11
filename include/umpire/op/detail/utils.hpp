//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#pragma once

#include <cstddef>
#include <limits>
#include <type_traits>

#include "umpire/util/error.hpp"

namespace umpire {
namespace op {
namespace detail {

/**
 * @brief Calculate size in bytes based on element count and type
 *
 * @tparam T The element type (e.g. float, or void for raw bytes)
 * @param count Number of elements or bytes (if T is void)
 * @return std::size_t Size in bytes
 */
template <typename T>
inline std::size_t get_size(std::size_t count)
{
  if constexpr (std::is_same_v<T, void>) {
    return count;
  } else {
    if (count > (std::numeric_limits<std::size_t>::max() / sizeof(T))) {
      UMPIRE_ERROR(runtime_error, fmt::format("Requested size overflow: count ({}) * sizeof(T) ({}) exceeds size_t max",
                                              count, sizeof(T)));
    }
    return count * sizeof(T);
  }
}

} // namespace detail
} // namespace op
} // namespace umpire
