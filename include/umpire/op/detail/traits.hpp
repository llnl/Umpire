#pragma once

#include <type_traits>

#include "umpire/config.hpp"
#include "umpire/resource/platform.hpp"

namespace umpire {
namespace op {
namespace detail {

template <typename Platform>
struct supports_memory_advice : std::false_type {};

#if defined(UMPIRE_ENABLE_CUDA)
template <>
struct supports_memory_advice<resource::cuda_platform> : std::true_type {};
#endif

#if defined(UMPIRE_ENABLE_HIP)
template <>
struct supports_memory_advice<resource::hip_platform> : std::true_type {};
#endif

} // namespace detail
} // namespace op
} // namespace umpire
