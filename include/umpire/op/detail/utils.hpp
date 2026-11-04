#pragma once

#include <cstddef>
#include <limits>
#include <type_traits>
#include "camp/resource.hpp"
#include "umpire/util/error.hpp"

namespace umpire {
namespace op {
namespace detail {

/**
 * @brief Calculate size in bytes based on element count and type
 *
 * @tparam T The pointer type (void* or typed pointer)
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
      UMPIRE_ERROR(runtime_error,
                   fmt::format("Requested size overflow: count ({}) * sizeof(T) ({}) exceeds size_t max", count,
                               sizeof(T)));
    }
    return count * sizeof(T);
  }
}

/**
 * @brief Create a default event for platforms without native async support
 * 
 * @param resource The resource to create the event for
 * @return camp::resources::EventProxy<camp::resources::Resource> A completed event
 */
inline camp::resources::EventProxy<camp::resources::Resource> 
make_completed_event(camp::resources::Resource& resource) noexcept
{
  return camp::resources::EventProxy<camp::resources::Resource>{resource};
}

/**
 * @brief Get minimum of two values (used for copy size calculations)
 * 
 * @tparam T The type of values being compared
 * @param a First value
 * @param b Second value
 * @return constexpr T The smaller of the two values
 */
template <typename T>
constexpr T min(const T& a, const T& b) noexcept
{
  return (a < b) ? a : b;
}

} // namespace detail
} // namespace op
} // namespace umpire
