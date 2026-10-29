#pragma once

#include <cuda_runtime.h>
#include <type_traits>

#include "umpire/op/detail/utils.hpp"
#include "umpire/op/operations.hpp"
#include "umpire/resource/platform.hpp"
#include "umpire/util/Platform.hpp"
#include "umpire/util/error.hpp"

namespace umpire {
namespace op {

// CUDA implementation helpers
namespace detail {

#if defined(__CUDACC__)
template <typename T>
__global__ void umpire_device_memset_kernel(T* data, T value, std::size_t count);
#endif

template <typename T>
void device_memset(T* ptr, T value, std::size_t count);

/**
 * @brief Get the CUDA memory copy direction kind
 *
 * @tparam SRC Source platform
 * @tparam DST Destination platform
 */
template <typename SRC, typename DST>
struct copy_kind;

// Device to host specialization
template <>
struct copy_kind<resource::cuda_platform, resource::host_platform> {
  static constexpr cudaMemcpyKind value = cudaMemcpyDeviceToHost;
};

// Host to device specialization
template <>
struct copy_kind<resource::host_platform, resource::cuda_platform> {
  static constexpr cudaMemcpyKind value = cudaMemcpyHostToDevice;
};

// Device to device specialization
template <>
struct copy_kind<resource::cuda_platform, resource::cuda_platform> {
  static constexpr cudaMemcpyKind value = cudaMemcpyDeviceToDevice;
};

/**
 * @brief Check if a CUDA device supports managed memory features
 *
 * @param device Device ID to check
 * @return true if the device supports managed memory
 * @return false if the device does not support managed memory
 */
inline bool supports_managed_memory(int device)
{
  cudaDeviceProp properties;
  cudaError_t error = ::cudaGetDeviceProperties(&properties, device);

  if (error != cudaSuccess) {
    UMPIRE_ERROR(runtime_error, fmt::format("cudaGetDeviceProperties for device {} failed with error: {}", device,
                                            cudaGetErrorString(error)));
  }

  return (properties.managedMemory == 1 && properties.concurrentManagedAccess == 1);
}

/**
 * @brief Get CUDA stream from a resource
 *
 * @param resource The resource to get the stream from
 * @return cudaStream_t The CUDA stream
 */
inline cudaStream_t get_stream(camp::resources::Resource& resource)
{
  auto cuda_resource = resource.try_get<camp::resources::Cuda>();
  if (!cuda_resource) {
    UMPIRE_ERROR(resource_error, fmt::format("Expected resources::Cuda, got resources::{}",
                                             platform_to_string(resource.get_platform())));
  }
  return cuda_resource->get_stream();
}

/**
 * @brief Apply memory advice to a CUDA managed memory allocation
 *
 * Advise is a performance hint, not a correctness requirement, so this is
 * intentionally a no-op (with a logged warning) on devices that do not
 * support managed memory.
 *
 * @tparam T Type of memory
 * @param ptr Pointer to memory
 * @param count Number of elements
 * @param device Device ID for advice
 * @param advice Memory advice to apply
 */
template <typename T>
inline void advise(T* ptr, std::size_t count, int device, cudaMemoryAdvise advice)
{
  // device may be cudaCpuDeviceId (-1, e.g. preferred-location-host); that is
  // not a valid argument to cudaGetDeviceProperties, so check managed-memory
  // support against the current device instead (mirrors the prefetch path).
  int gpu = device;
  if (device == cudaCpuDeviceId) {
    cudaError_t get_dev_err = cudaGetDevice(&gpu);
    if (get_dev_err != cudaSuccess) {
      UMPIRE_ERROR(runtime_error, fmt::format("cudaGetDevice failed: {} ({})", cudaGetErrorString(get_dev_err),
                                              static_cast<int>(get_dev_err)));
    }
  }

  // Skip if device doesn't support managed memory
  if (!supports_managed_memory(gpu)) {
    UMPIRE_LOG(Warning, "cudaMemAdvise skipped: device " << gpu << " does not support managed memory");
    return;
  }

  std::size_t size = detail::get_size<T>(count);
#if CUDART_VERSION >= 13000
  cudaMemLocation loc = {(device == cudaCpuDeviceId) ? cudaMemLocationTypeHost : cudaMemLocationTypeDevice, device};
  cudaError_t error = ::cudaMemAdvise(ptr, size, advice, loc);
#else
  cudaError_t error = ::cudaMemAdvise(ptr, size, advice, device);
#endif

  if (error != cudaSuccess) {
    UMPIRE_ERROR(runtime_error,
                 fmt::format("cudaMemAdvise(ptr={}, size={}, advice={}, device={}) failed with error: {}",
                             fmt::ptr(ptr), size, static_cast<int>(advice), device, cudaGetErrorString(error)));
  }
}

/**
 * @brief Synchronous memory copy implementation
 *
 * @tparam T Type of memory
 * @param src Source pointer
 * @param dst Destination pointer
 * @param count Number of elements
 * @param kind Copy direction kind
 */
template <typename T>
inline void copy(T* src, T* dst, std::size_t count, cudaMemcpyKind kind)
{
  std::size_t size = detail::get_size<T>(count);

  cudaError_t error = ::cudaMemcpy(dst, src, size, kind);
  if (error != cudaSuccess) {
    UMPIRE_ERROR(runtime_error, fmt::format("cudaMemcpy(dst={}, src={}, size={}, kind={}) failed with error: {}", reinterpret_cast<void*>(dst),
                                            reinterpret_cast<void*>(src), size, static_cast<int>(kind), cudaGetErrorString(error)));
  }
}

/**
 * @brief Asynchronous memory copy implementation
 *
 * @tparam T Type of memory
 * @param src Source pointer
 * @param dst Destination pointer
 * @param count Number of elements
 * @param resource Resource for asynchronous operation
 * @param kind Copy direction kind
 * @return Event representing the asynchronous operation
 */
template <typename T>
inline camp::resources::EventProxy<camp::resources::Resource> copy_async(T* src, T* dst, std::size_t count,
                                                                         camp::resources::Resource& resource,
                                                                         cudaMemcpyKind kind)
{
  auto stream = get_stream(resource);
  std::size_t size = detail::get_size<T>(count);

  cudaError_t error = ::cudaMemcpyAsync(dst, src, size, kind, stream);
  if (error != cudaSuccess) {
    UMPIRE_ERROR(runtime_error,
                 fmt::format("cudaMemcpyAsync(dst={}, src={}, size={}, kind={}, stream={}) failed with error: {}",
                             fmt::ptr(dst), fmt::ptr(src), size, static_cast<int>(kind), static_cast<void*>(stream),
                             cudaGetErrorString(error)));
  }

  return camp::resources::EventProxy<camp::resources::Resource>{resource};
}

/**
 * @brief Synchronous memory set implementation
 *
 * @tparam T Type of memory
 * @param ptr Pointer to memory
 * @param value Value to set
 * @param count Number of elements
 */
template <typename T>
inline void memset(T* ptr, int value, std::size_t count)
{
  std::size_t size = detail::get_size<T>(count);

  cudaError_t error = ::cudaMemset(ptr, value, size);
  if (error != cudaSuccess) {
    UMPIRE_ERROR(runtime_error, fmt::format("cudaMemset(ptr={}, value={}, size={}) failed with error: {}", reinterpret_cast<void*>(ptr), value,
                                            size, cudaGetErrorString(error)));
  }
}

/**
 * @brief Asynchronous memory set implementation
 *
 * @tparam T Type of memory
 * @param ptr Pointer to memory
 * @param value Value to set
 * @param count Number of elements
 * @param resource Resource for asynchronous operation
 * @return Event representing the asynchronous operation
 */
template <typename T>
inline camp::resources::EventProxy<camp::resources::Resource> memset_async(T* ptr, int value, std::size_t count,
                                                                           camp::resources::Resource& resource)
{
  auto stream = get_stream(resource);
  std::size_t size = detail::get_size<T>(count);

  cudaError_t error = ::cudaMemsetAsync(ptr, value, size, stream);
  if (error != cudaSuccess) {
    UMPIRE_ERROR(runtime_error,
                 fmt::format("cudaMemsetAsync(ptr={}, value={}, size={}, stream={}) failed with error: {}",
                             fmt::ptr(ptr), value, size, static_cast<void*>(stream), cudaGetErrorString(error)));
  }

  return camp::resources::EventProxy<camp::resources::Resource>{resource};
}

/**
 * @brief Synchronous memory prefetch implementation
 *
 * @tparam T Type of memory
 * @param ptr Pointer to memory
 * @param device Device to prefetch to
 * @param count Number of elements
 */
template <typename T>
inline void prefetch(T* ptr, int device, std::size_t count)
{
  // Use current device for properties if device is CPU
  int current_device;
  cudaError_t get_dev_err = cudaGetDevice(&current_device);
  if (get_dev_err != cudaSuccess) {
    UMPIRE_ERROR(runtime_error,
                 fmt::format("cudaGetDevice failed: {} ({})", cudaGetErrorString(get_dev_err),
                             static_cast<int>(get_dev_err)));
  }
  int gpu = (device != cudaCpuDeviceId) ? device : current_device;
#if CUDART_VERSION >= 13000
  cudaMemLocation loc = {(device == cudaCpuDeviceId) ? cudaMemLocationTypeHost : cudaMemLocationTypeDevice, device};
#endif

  if (supports_managed_memory(gpu)) {
    std::size_t size = detail::get_size<T>(count);
#if CUDART_VERSION >= 13000
    cudaError_t error = ::cudaMemPrefetchAsync(ptr, size, loc, 0, nullptr);
#else
    cudaError_t error = ::cudaMemPrefetchAsync(ptr, size, device, nullptr);
#endif

    if (error != cudaSuccess) {
      UMPIRE_ERROR(runtime_error, fmt::format("cudaMemPrefetchAsync(ptr={}, size={}, device={}) failed with error: {}",
                                              reinterpret_cast<void*>(ptr), size, device, cudaGetErrorString(error)));
    }
  }
}

/**
 * @brief Asynchronous memory prefetch implementation
 *
 * @tparam T Type of memory
 * @param ptr Pointer to memory
 * @param device Device to prefetch to
 * @param count Number of elements
 * @param resource Resource for asynchronous operation
 * @return Event representing the asynchronous operation
 */
template <typename T>
inline camp::resources::EventProxy<camp::resources::Resource> prefetch_async(T* ptr, int device, std::size_t count,
                                                                             camp::resources::Resource& resource)
{
  auto stream = get_stream(resource);

  // Use current device for properties if device is CPU
  int current_device;
  cudaError_t get_dev_err = cudaGetDevice(&current_device);
  if (get_dev_err != cudaSuccess) {
    UMPIRE_ERROR(runtime_error,
                 fmt::format("cudaGetDevice failed: {} ({})", cudaGetErrorString(get_dev_err),
                             static_cast<int>(get_dev_err)));
  }
  int gpu = (device != cudaCpuDeviceId) ? device : current_device;
#if CUDART_VERSION >= 13000
  cudaMemLocation loc = {(device == cudaCpuDeviceId) ? cudaMemLocationTypeHost : cudaMemLocationTypeDevice, device};
#endif

  if (supports_managed_memory(gpu)) {
    std::size_t size = detail::get_size<T>(count);
#if CUDART_VERSION >= 13000
    cudaError_t error = ::cudaMemPrefetchAsync(ptr, size, loc, 0, stream);
#else
    cudaError_t error = ::cudaMemPrefetchAsync(ptr, size, device, stream);
#endif

    if (error != cudaSuccess) {
      UMPIRE_ERROR(runtime_error,
                   fmt::format("cudaMemPrefetchAsync(ptr={}, size={}, device={}, stream={}) failed with error: {}",
                               fmt::ptr(ptr), size, device, static_cast<void*>(stream), cudaGetErrorString(error)));
    }
  }

  return camp::resources::EventProxy<camp::resources::Resource>{resource};
}

} // namespace detail

//------------------------------------------------------------------------------
// CUDA Operation Template Specializations
//------------------------------------------------------------------------------

// CUDA-to-CUDA copy operation
template <>
struct copy<resource::cuda_platform, resource::cuda_platform> {
  /**
   * @brief CUDA to CUDA synchronous copy
   *
   * @tparam T Type of data being copied
   * @param src Source pointer
   * @param dst Destination pointer
   * @param len Number of elements to copy
   */
  template <typename T>
  static void exec(T* src, T* dst, std::size_t len)
  {
    detail::copy(src, dst, len, detail::copy_kind<resource::cuda_platform, resource::cuda_platform>::value);
  }

  /**
   * @brief CUDA to CUDA asynchronous copy
   *
   * @tparam T Type of data being copied
   * @param src Source pointer
   * @param dst Destination pointer
   * @param len Number of elements to copy
   * @param resource Resource for asynchronous operation
   * @return Event representing the asynchronous operation
   */
  template <typename T>
  static camp::resources::EventProxy<camp::resources::Resource> exec(T* src, T* dst, std::size_t len,
                                                                     camp::resources::Resource& resource)
  {
    return detail::copy_async(src, dst, len, resource,
                              detail::copy_kind<resource::cuda_platform, resource::cuda_platform>::value);
  }
};

// CUDA-to-Host copy operation
template <>
struct copy<resource::cuda_platform, resource::host_platform> {
  /**
   * @brief CUDA to Host synchronous copy
   *
   * @tparam T Type of data being copied
   * @param src Source pointer
   * @param dst Destination pointer
   * @param len Number of elements to copy
   */
  template <typename T>
  static void exec(T* src, T* dst, std::size_t len)
  {
    detail::copy(src, dst, len, detail::copy_kind<resource::cuda_platform, resource::host_platform>::value);
  }

  /**
   * @brief CUDA to Host asynchronous copy
   *
   * @tparam T Type of data being copied
   * @param src Source pointer
   * @param dst Destination pointer
   * @param len Number of elements to copy
   * @param resource Resource for asynchronous operation
   * @return Event representing the asynchronous operation
   */
  template <typename T>
  static camp::resources::EventProxy<camp::resources::Resource> exec(T* src, T* dst, std::size_t len,
                                                                     camp::resources::Resource& resource)
  {
    return detail::copy_async(src, dst, len, resource,
                              detail::copy_kind<resource::cuda_platform, resource::host_platform>::value);
  }
};

// Host-to-CUDA copy operation
template <>
struct copy<resource::host_platform, resource::cuda_platform> {
  /**
   * @brief Host to CUDA synchronous copy
   *
   * @tparam T Type of data being copied
   * @param src Source pointer
   * @param dst Destination pointer
   * @param len Number of elements to copy
   */
  template <typename T>
  static void exec(T* src, T* dst, std::size_t len)
  {
    detail::copy(src, dst, len, detail::copy_kind<resource::host_platform, resource::cuda_platform>::value);
  }

  /**
   * @brief Host to CUDA asynchronous copy
   *
   * @tparam T Type of data being copied
   * @param src Source pointer
   * @param dst Destination pointer
   * @param len Number of elements to copy
   * @param resource Resource for asynchronous operation
   * @return Event representing the asynchronous operation
   */
  template <typename T>
  static camp::resources::EventProxy<camp::resources::Resource> exec(T* src, T* dst, std::size_t len,
                                                                     camp::resources::Resource& resource)
  {
    return detail::copy_async(src, dst, len, resource,
                              detail::copy_kind<resource::host_platform, resource::cuda_platform>::value);
  }
};

// CUDA memset operation
template <>
struct memset<resource::cuda_platform> {
  /**
   * @brief CUDA synchronous memset
   *
   * @tparam T Type of memory being set
   * @param ptr Pointer to memory
   * @param val Value to set
   * @param len Number of elements to set
   */
  template <typename T>
  static void exec(T* ptr, int val, std::size_t len)
  {
    detail::memset(ptr, val, len);
  }

  /**
   * @brief CUDA asynchronous memset
   *
   * @tparam T Type of memory being set
   * @param ptr Pointer to memory
   * @param val Value to set
   * @param len Number of elements to set
   * @param resource Resource for asynchronous operation
   * @return Event representing the asynchronous operation
   */
  template <typename T>
  static camp::resources::EventProxy<camp::resources::Resource> exec(T* ptr, int val, std::size_t len,
                                                                     camp::resources::Resource& resource)
  {
    return detail::memset_async(ptr, val, len, resource);
  }
};

// CUDA prefetch operation
template <>
struct prefetch<resource::cuda_platform> {
  /**
   * @brief CUDA synchronous prefetch
   *
   * @tparam T Type of memory being prefetched
   * @param ptr Pointer to memory
   * @param device Device to prefetch to
   * @param len Number of elements to prefetch
   */
  template <typename T>
  static void exec(T* ptr, int device, std::size_t len)
  {
    detail::prefetch(ptr, device, len);
  }

  /**
   * @brief CUDA asynchronous prefetch
   *
   * @tparam T Type of memory being prefetched
   * @param ptr Pointer to memory
   * @param device Device to prefetch to
   * @param len Number of elements to prefetch
   * @param resource Resource for asynchronous operation
   * @return Event representing the asynchronous operation
   */
  template <typename T>
  static camp::resources::EventProxy<camp::resources::Resource> exec(T* ptr, int device, std::size_t len,
                                                                     camp::resources::Resource& resource)
  {
    return detail::prefetch_async(ptr, device, len, resource);
  }
};

// CUDA device_memset operation
template <>
struct device_memset<resource::cuda_platform> {
  template <typename T>
  static void exec(T* ptr, T val, std::size_t len)
  {
    detail::device_memset(ptr, val, len);
  }

  // Async overload for API parity: the underlying kernel wrapper synchronizes
  // the device before returning, so the returned event represents completed
  // work.
  template <typename T>
  static camp::resources::EventProxy<camp::resources::Resource> exec(T* ptr, T val, std::size_t len,
                                                                     camp::resources::Resource& res)
  {
    detail::device_memset(ptr, val, len);
    return camp::resources::EventProxy<camp::resources::Resource>{res};
  }
};

// Memory advice operations define macro to reduce duplication
#define DEFINE_CUDA_ADVICE_OP(op_name, advice_flag)                       \
  template <>                                                             \
  struct op_name<resource::cuda_platform> {                               \
    /**                                                                   \
     * @brief Apply memory advice operation                               \
     *                                                                    \
     * @tparam T Type of memory                                           \
     * @param ptr Pointer to memory                                       \
     * @param device Device to apply advice for                           \
     * @param len Number of elements                                      \
     */                                                                   \
    template <typename T>                                                 \
    static inline void exec(T* ptr, int device, std::size_t len)          \
    {                                                                     \
      detail::advise(ptr, len, device, advice_flag);                      \
    }                                                                     \
  };

DEFINE_CUDA_ADVICE_OP(set_accessed_by, cudaMemAdviseSetAccessedBy)
DEFINE_CUDA_ADVICE_OP(set_preferred_location, cudaMemAdviseSetPreferredLocation)
DEFINE_CUDA_ADVICE_OP(set_read_mostly, cudaMemAdviseSetReadMostly)
DEFINE_CUDA_ADVICE_OP(unset_accessed_by, cudaMemAdviseUnsetAccessedBy)
DEFINE_CUDA_ADVICE_OP(unset_preferred_location, cudaMemAdviseUnsetPreferredLocation)
DEFINE_CUDA_ADVICE_OP(unset_read_mostly, cudaMemAdviseUnsetReadMostly)

#undef DEFINE_CUDA_ADVICE_OP

} // namespace op
} // namespace umpire
