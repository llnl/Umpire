#pragma once

#include "camp/resource/platform.hpp"
#include "umpire/config.hpp"

namespace umpire {
namespace resource {

template <typename Platform>
struct platform_for {};

struct undefined_platform {};
struct host_platform {};
#if defined(UMPIRE_ENABLE_CUDA)
struct cuda_platform {};
#endif
#if defined(UMPIRE_ENABLE_HIP)
struct hip_platform {};
#endif
#if defined(UMPIRE_ENABLE_OPENMP_TARGET)
struct omp_target_platform {};
#endif
#if defined(UMPIRE_ENABLE_SYCL)
struct sycl_platform {};
#endif

template <>
struct platform_for<undefined_platform> {
  static constexpr camp::resources::Platform value = camp::resources::Platform::undefined;
};

template <>
struct platform_for<host_platform> {
  static constexpr camp::resources::Platform value = camp::resources::Platform::host;
};

#if defined(UMPIRE_ENABLE_CUDA)
template <>
struct platform_for<cuda_platform> {
  static constexpr camp::resources::Platform value = camp::resources::Platform::cuda;
};
#endif
#if defined(UMPIRE_ENABLE_HIP)
template <>
struct platform_for<hip_platform> {
  static constexpr camp::resources::Platform value = camp::resources::Platform::hip;
};
#endif
#if defined(UMPIRE_ENABLE_OPENMP_TARGET)
template <>
struct platform_for<omp_target_platform> {
  static constexpr camp::resources::Platform value = camp::resources::Platform::omp_target;
};
#endif
#if defined(UMPIRE_ENABLE_SYCL)
template <>
struct platform_for<sycl_platform> {
  static constexpr camp::resources::Platform value = camp::resources::Platform::sycl;
};
#endif

} // namespace resource

using host = resource::host_platform;
#if defined(UMPIRE_ENABLE_CUDA)
using cuda = resource::cuda_platform;
#endif
#if defined(UMPIRE_ENABLE_HIP)
using hip = resource::hip_platform;
#endif
#if defined(UMPIRE_ENABLE_OPENMP_TARGET)
using omp_target = resource::omp_target_platform;
#endif
// No `umpire::sycl` alias: inside `namespace umpire` it would shadow the
// SYCL standard library's global `::sycl` namespace, breaking unqualified
// `sycl::queue`/`sycl::event` references throughout umpire's SYCL code.
// Use umpire::resource::sycl_platform directly instead.

} // namespace umpire
