//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#pragma once

#include <hip/hip_runtime.h>

#include <type_traits>

#include "umpire/op/detail/utils.hpp"
#include "umpire/op/operations.hpp"
#include "umpire/resource/platform.hpp"
#include "umpire/util/Platform.hpp"
#include "umpire/util/error.hpp"

namespace umpire {
namespace op {

namespace detail {
template <typename T>
__global__ void umpire_device_memset_kernel(T* data, T value, std::size_t count);
}

// HIP implementation helpers
namespace detail {

/**
 * @brief Check if a HIP device supports managed memory features
 *
 * @param device Device ID to check
 * @return true if the device supports managed memory
 * @return false if the device does not support managed memory
 */
inline bool supports_managed_memory(int device)
{
  hipDeviceProp_t properties;
  hipError_t error = ::hipGetDeviceProperties(&properties, device);

  if (error != hipSuccess) {
    UMPIRE_ERROR(runtime_error, fmt::format("hipGetDeviceProperties for device {} failed with error: {}", device,
                                            hipGetErrorString(error)));
  }

  return (properties.managedMemory == 1 && properties.concurrentManagedAccess == 1);
}

/**
 * @brief Get HIP stream from a resource
 *
 * @param resource The resource to get the stream from
 * @return hipStream_t The HIP stream
 */
inline hipStream_t get_stream(camp::resources::Resource& resource)
{
  auto hip_resource = resource.try_get<camp::resources::Hip>();
  if (!hip_resource) {
    UMPIRE_ERROR(resource_error, fmt::format("Expected resources::Hip, got resources::{}",
                                             platform_to_string(resource.get_platform())));
  }
  return hip_resource->get_stream();
}

/**
 * @brief Apply memory advice to a HIP managed memory allocation
 *
 * @note advise() is a performance hint, not a correctness requirement. On
 * devices that do not support managed memory, this is intentionally a
 * logged no-op rather than an error.
 *
 * @tparam T Type of memory
 * @param ptr Pointer to memory
 * @param count Number of elements
 * @param device Device ID for advice
 * @param advice Memory advice to apply
 */
template <typename T>
inline void advise(T* ptr, std::size_t count, int device, hipMemoryAdvise advice)
{
  // device may be hipCpuDeviceId (-1, e.g. preferred-location-host); that is
  // not a valid argument to hipGetDeviceProperties, so check managed-memory
  // support against the current device instead (mirrors the prefetch path).
  int gpu = device;
  if (device == hipCpuDeviceId) {
    hipError_t get_dev_err = hipGetDevice(&gpu);
    if (get_dev_err != hipSuccess) {
      UMPIRE_ERROR(runtime_error, fmt::format("hipGetDevice failed with error: {}", hipGetErrorString(get_dev_err)));
    }
  }

  // Skip if device doesn't support managed memory
  if (!supports_managed_memory(gpu)) {
    UMPIRE_LOG(Warning, "hipMemAdvise skipped: device " << gpu << " does not support managed memory");
    return;
  }

  std::size_t size = detail::get_size<T>(count);
  hipError_t error = ::hipMemAdvise(ptr, size, advice, device);

  if (error != hipSuccess) {
    UMPIRE_ERROR(runtime_error, fmt::format("hipMemAdvise(ptr={}, size={}, advice={}, device={}) failed with error: {}",
                                            reinterpret_cast<void*>(ptr), size, static_cast<int>(advice), device,
                                            hipGetErrorString(error)));
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
inline void copy(T* src, T* dst, std::size_t count, hipMemcpyKind kind)
{
  std::size_t size = detail::get_size<T>(count);

  hipError_t error = ::hipMemcpy(dst, src, size, kind);
  if (error != hipSuccess) {
    UMPIRE_ERROR(runtime_error, fmt::format("hipMemcpy(dst={}, src={}, size={}, kind={}) failed with error: {}",
                                            reinterpret_cast<void*>(dst), reinterpret_cast<void*>(src), size,
                                            static_cast<int>(kind), hipGetErrorString(error)));
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
                                                                         hipMemcpyKind kind)
{
  auto stream = get_stream(resource);
  std::size_t size = detail::get_size<T>(count);

  hipError_t error = ::hipMemcpyAsync(dst, src, size, kind, stream);
  if (error != hipSuccess) {
    UMPIRE_ERROR(
        runtime_error,
        fmt::format("hipMemcpyAsync(dst={}, src={}, size={}, kind={}, stream={}) failed with error: {}", fmt::ptr(dst),
                    fmt::ptr(src), size, static_cast<int>(kind), static_cast<void*>(stream), hipGetErrorString(error)));
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

  hipError_t error = ::hipMemset(ptr, value, size);
  if (error != hipSuccess) {
    UMPIRE_ERROR(runtime_error, fmt::format("hipMemset(ptr={}, value={}, size={}) failed with error: {}",
                                            reinterpret_cast<void*>(ptr), value, size, hipGetErrorString(error)));
  }
}

/**
 * @brief Element types for which device_memset is explicitly instantiated
 *
 * Must be kept in sync with the explicit instantiation lists in
 * src/umpire/op/hip.cpp.
 */
template <typename T>
inline constexpr bool is_device_memset_type_v =
    std::is_same_v<T, char> || std::is_same_v<T, signed char> || std::is_same_v<T, unsigned char> ||
    std::is_same_v<T, short> || std::is_same_v<T, unsigned short> || std::is_same_v<T, int> ||
    std::is_same_v<T, unsigned int> || std::is_same_v<T, long> || std::is_same_v<T, unsigned long> ||
    std::is_same_v<T, long long> || std::is_same_v<T, unsigned long long> || std::is_same_v<T, float> ||
    std::is_same_v<T, double> || std::is_same_v<T, bool>;

/**
 * @brief Synchronous memory set implementation using device kernel
 *
 * Sets each element in the array to the specified value using a HIP kernel
 * Unlike standard memset which operates on bytes, this operates on typed elements.
 * Launches on the device owning ptr (from hipPointerGetAttributes) and
 * synchronizes the launch stream before returning. The current device is
 * restored afterward.
 *
 * @tparam T Type of array elements
 * @param ptr Pointer to array
 * @param value Value to set each element to
 * @param count Number of elements to set
 */
template <typename T>
void device_memset(T* ptr, T value, std::size_t count);

/**
 * @brief Asynchronous memory set implementation using device kernel
 *
 * Launches on the given stream and device without synchronizing. The current
 * device is restored afterward.
 *
 * @tparam T Type of array elements
 * @param ptr Pointer to array
 * @param value Value to set each element to
 * @param count Number of elements to set
 * @param stream Stream to launch on
 * @param device Device to launch on
 */
template <typename T>
void device_memset(T* ptr, T value, std::size_t count, hipStream_t stream, int device);

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

  hipError_t error = ::hipMemsetAsync(ptr, value, size, stream);
  if (error != hipSuccess) {
    UMPIRE_ERROR(runtime_error,
                 fmt::format("hipMemsetAsync(ptr={}, value={}, size={}, stream={}) failed with error: {}",
                             fmt::ptr(ptr), value, size, static_cast<void*>(stream), hipGetErrorString(error)));
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
  hipError_t get_dev_err = hipGetDevice(&current_device);

  if (get_dev_err != hipSuccess) {
    UMPIRE_ERROR(runtime_error, fmt::format("hipGetDevice failed with error: {}", hipGetErrorString(get_dev_err)));
  }

  int gpu = (device != hipCpuDeviceId) ? device : current_device;

  if (supports_managed_memory(gpu)) {
    std::size_t size = detail::get_size<T>(count);
    hipError_t error = ::hipMemPrefetchAsync(ptr, size, device, nullptr);

    if (error != hipSuccess) {
      UMPIRE_ERROR(runtime_error, fmt::format("hipMemPrefetchAsync(ptr={}, size={}, device={}) failed with error: {}",
                                              reinterpret_cast<void*>(ptr), size, device, hipGetErrorString(error)));
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
  hipError_t get_dev_err = hipGetDevice(&current_device);

  if (get_dev_err != hipSuccess) {
    UMPIRE_ERROR(runtime_error, fmt::format("hipGetDevice failed with error: {}", hipGetErrorString(get_dev_err)));
  }

  int gpu = (device != hipCpuDeviceId) ? device : current_device;

  if (supports_managed_memory(gpu)) {
    std::size_t size = detail::get_size<T>(count);
    hipError_t error = ::hipMemPrefetchAsync(ptr, size, device, stream);

    if (error != hipSuccess) {
      UMPIRE_ERROR(runtime_error,
                   fmt::format("hipMemPrefetchAsync(ptr={}, size={}, device={}, stream={}) failed with error: {}",
                               fmt::ptr(ptr), size, device, static_cast<void*>(stream), hipGetErrorString(error)));
    }
  }

  return camp::resources::EventProxy<camp::resources::Resource>{resource};
}

} // namespace detail

//------------------------------------------------------------------------------
// HIP Operation Template Specializations
//------------------------------------------------------------------------------

// HIP-to-HIP copy operation
template <>
struct copy<resource::hip_platform, resource::hip_platform> {
  /**
   * @brief HIP to HIP synchronous copy
   *
   * @tparam T Type of data being copied
   * @param src Source pointer
   * @param dst Destination pointer
   * @param len Number of elements to copy
   */
  template <typename T>
  static void exec(T* src, T* dst, std::size_t len)
  {
    detail::copy(src, dst, len, hipMemcpyDeviceToDevice);
  }

  /**
   * @brief HIP to HIP asynchronous copy
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
    return detail::copy_async(src, dst, len, resource, hipMemcpyDeviceToDevice);
  }
};

// HIP-to-Host copy operation
template <>
struct copy<resource::hip_platform, resource::host_platform> {
  /**
   * @brief HIP to Host synchronous copy
   *
   * @tparam T Type of data being copied
   * @param src Source pointer
   * @param dst Destination pointer
   * @param len Number of elements to copy
   */
  template <typename T>
  static void exec(T* src, T* dst, std::size_t len)
  {
    detail::copy(src, dst, len, hipMemcpyDeviceToHost);
  }

  /**
   * @brief HIP to Host asynchronous copy
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
    return detail::copy_async(src, dst, len, resource, hipMemcpyDeviceToHost);
  }
};

// Host-to-HIP copy operation
template <>
struct copy<resource::host_platform, resource::hip_platform> {
  /**
   * @brief Host to HIP synchronous copy
   *
   * @tparam T Type of data being copied
   * @param src Source pointer
   * @param dst Destination pointer
   * @param len Number of elements to copy
   */
  template <typename T>
  static void exec(T* src, T* dst, std::size_t len)
  {
    detail::copy(src, dst, len, hipMemcpyHostToDevice);
  }

  /**
   * @brief Host to HIP asynchronous copy
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
    return detail::copy_async(src, dst, len, resource, hipMemcpyHostToDevice);
  }
};

// HIP memset operation
template <>
struct memset<resource::hip_platform> {
  /**
   * @brief HIP synchronous memset
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
   * @brief HIP asynchronous memset
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

// HIP device memset operation
template <>
struct device_memset<resource::hip_platform> {
  /**
   * @brief HIP synchronous device memset using kernel
   *
   * @tparam T Type of array elements
   * @param ptr Pointer to array
   * @param val Value to set each element to
   * @param len Number of elements to set
   */
  template <typename T, typename V>
  static void exec(T* ptr, V val, std::size_t len)
  {
    static_assert(detail::is_device_memset_type_v<T>,
                  "umpire::device_memset (HIP) supports only: char, signed char, unsigned char, short, "
                  "unsigned short, int, unsigned int, long, unsigned long, long long, unsigned long long, "
                  "float, double, bool");
    detail::device_memset<T>(ptr, static_cast<T>(val), len);
  }

  // Async overload: launches on the resource's stream and device without
  // synchronizing; the returned event tracks completion.
  template <typename T, typename V>
  static camp::resources::EventProxy<camp::resources::Resource> exec(T* ptr, V val, std::size_t len,
                                                                     camp::resources::Resource& res)
  {
    static_assert(detail::is_device_memset_type_v<T>,
                  "umpire::device_memset (HIP) supports only: char, signed char, unsigned char, short, "
                  "unsigned short, int, unsigned int, long, unsigned long, long long, unsigned long long, "
                  "float, double, bool");
    auto stream = detail::get_stream(res);
    int device = res.get<camp::resources::Hip>().get_device();
    detail::device_memset<T>(ptr, static_cast<T>(val), len, stream, device);
    return camp::resources::EventProxy<camp::resources::Resource>{res};
  }
};

// HIP prefetch operation
template <>
struct prefetch<resource::hip_platform> {
  /**
   * @brief HIP synchronous prefetch
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
   * @brief HIP asynchronous prefetch
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

// Memory advice operations - using macro to reduce duplication
#define DEFINE_HIP_ADVICE_OP(op_name, advice_flag)               \
  template <>                                                    \
  struct op_name<resource::hip_platform> {                       \
    /**                                                          \
     * @brief Apply memory advice operation                      \
     *                                                           \
     * @tparam T Type of memory                                  \
     * @param ptr Pointer to memory                              \
     * @param device Device to apply advice for                  \
     * @param len Number of elements                             \
     */                                                          \
    template <typename T>                                        \
    static inline void exec(T* ptr, int device, std::size_t len) \
    {                                                            \
      detail::advise(ptr, len, device, advice_flag);             \
    }                                                            \
  };

DEFINE_HIP_ADVICE_OP(set_accessed_by, hipMemAdviseSetAccessedBy)
DEFINE_HIP_ADVICE_OP(set_preferred_location, hipMemAdviseSetPreferredLocation)
DEFINE_HIP_ADVICE_OP(set_read_mostly, hipMemAdviseSetReadMostly)
DEFINE_HIP_ADVICE_OP(unset_accessed_by, hipMemAdviseUnsetAccessedBy)
DEFINE_HIP_ADVICE_OP(unset_preferred_location, hipMemAdviseUnsetPreferredLocation)
DEFINE_HIP_ADVICE_OP(unset_read_mostly, hipMemAdviseUnsetReadMostly)

#if HIP_VERSION_MAJOR >= 5
DEFINE_HIP_ADVICE_OP(set_coarse_grain, hipMemAdviseSetCoarseGrain)
DEFINE_HIP_ADVICE_OP(unset_coarse_grain, hipMemAdviseUnsetCoarseGrain)
#endif

#undef DEFINE_HIP_ADVICE_OP

} // namespace op
} // namespace umpire
