//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-25, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#pragma once

#include "umpire/config.hpp"

#if defined(UMPIRE_ENABLE_OPENMP_TARGET)

#include <omp.h>
#include <iostream>
#include <memory>
#include <sstream>

#include "umpire/util/Platform.hpp"
#include "umpire/util/error.hpp"
#include "umpire/util/Macros.hpp"
#include "umpire/resource/platform.hpp"
#include "umpire/op/operations.hpp"
#include "umpire/op/detail/utils.hpp"
#include "umpire/ResourceManager.hpp"
#include "umpire/strategy/AllocationStrategy.hpp"
#include "camp/resource.hpp"
#include "camp/resource/event.hpp"

namespace umpire {
namespace op {

namespace detail {

/**
 * @brief Get the OpenMP target device id associated with a pointer
 *
 * Looks the pointer up in the Umpire allocation map and returns the device
 * id recorded in its allocation strategy's traits. This mirrors the legacy
 * OpenMPTargetCopyOperation/OpenMPTargetMemsetOperation, which derived the
 * device id from `allocation->strategy->getTraits().id`.
 *
 * @param ptr Pointer previously allocated/tracked by Umpire
 * @return The OpenMP device id associated with the pointer's allocation
 */
inline int get_device_id(const void* ptr)
{
  auto& rm = ResourceManager::getInstance();
  if (!rm.hasAllocator(const_cast<void*>(ptr))) {
    UMPIRE_ERROR(resource_error,
                 "OpenMP target operations on external device memory require an OpenMP resource context");
  }
  auto* record = rm.findAllocationRecord(const_cast<void*>(ptr));
  return record->strategy->getTraits().id;
}

/**
 * @brief Get the OpenMP device from a camp::resources::Resource
 *
 * Mirrors the get_stream()/get_queue() helpers used by the CUDA/HIP/SYCL
 * backends: validates that the resource is actually an Omp resource before
 * pulling the device id out of it.
 *
 * @param resource The resource to get the device from
 * @return The OpenMP device id of this resource
 */
inline int get_device(camp::resources::Resource& resource)
{
  auto omp_resource = resource.try_get<camp::resources::Omp>();
  if (!omp_resource) {
    UMPIRE_ERROR(resource_error, fmt::format("Expected resources::Omp, got resources::{}",
                                             platform_to_string(resource.get_platform())));
  }
  return omp_resource->get_device();
}

// A tracked allocation's device remains authoritative, including for copies
// between devices. The resource supplies the device for each external pointer.
inline int get_device_id(const void* ptr, camp::resources::Resource* resource)
{
  if (resource && !ResourceManager::getInstance().hasAllocator(const_cast<void*>(ptr))) {
    return get_device(*resource);
  }
  return get_device_id(ptr);
}

} // namespace detail

// OpenMP Target implementation helpers
//
// These live in a backend-specific namespace (rather than an anonymous one in
// umpire::op) so they cannot collide with the identically-shaped helpers in
// the other backend headers (e.g. op/sycl.hpp) when several backends are
// enabled in the same build.
namespace detail {
namespace omp_target_impl {
// Order an async op after the resource's pending work
//
// The async ops below execute synchronously on the host thread (see the
// "OpenMP Target Async Limitations" note), so they are not enqueued on `res`.
// Validate that `res` is actually an Omp resource (fail fast on mismatched
// resource types, matching the get_stream()/get_queue() pattern used by other
// backends), then wait on its pending work so the op is ordered after it.
inline void wait_for_resource(camp::resources::Resource& res)
{
  detail::get_device(res);
  res.get_event().wait();
}

// Helper function for device-to-device copy operations
//
// Resolve each device independently from allocation metadata, using the
// supplied resource's device for external pointers.
template <typename T>
inline void copy_impl(T* src_ptr, T* dst_ptr, std::size_t count,
                      camp::resources::Resource* res = nullptr) {
  std::size_t size = detail::get_size<T>(count);
  if (size == 0) {
    return;
  }

  int src_device = detail::get_device_id(src_ptr, res);
  int dst_device = detail::get_device_id(dst_ptr, res);

  UMPIRE_LOG(Debug, "omp_target_memcpy(dst_ptr = "
                        << static_cast<void*>(dst_ptr) << ", src_ptr = " << static_cast<void*>(src_ptr)
                        << ", length = " << size << ", src_id = " << src_device << ", dst_id = " << dst_device);

  omp_target_memcpy(static_cast<void*>(dst_ptr), static_cast<const void*>(src_ptr), size, 0, 0, dst_device,
                    src_device);
}

// Helper function for host-to-device copy operations
//
// The host side always targets omp_get_initial_device(); the device side uses
// allocation metadata or the supplied resource's device for external pointers.
template <typename T>
inline void host_to_device_copy_impl(T* src_ptr, T* dst_ptr, std::size_t count,
                                     camp::resources::Resource* res = nullptr) {
  std::size_t size = detail::get_size<T>(count);
  if (size == 0) {
    return;
  }

  int src_device = omp_get_initial_device();
  int dst_device = detail::get_device_id(dst_ptr, res);

  UMPIRE_LOG(Debug, "omp_target_memcpy(dst_ptr = "
                        << static_cast<void*>(dst_ptr) << ", src_ptr = " << static_cast<const void*>(src_ptr)
                        << ", length = " << size << ", src_id = " << src_device << ", dst_id = " << dst_device);

  omp_target_memcpy(static_cast<void*>(dst_ptr), static_cast<const void*>(src_ptr), size, 0, 0, dst_device,
                    src_device);
}

// Helper function for device-to-host copy operations
//
// Mirror of host_to_device_copy_impl: resolve the device side independently,
// while the host side always targets
// omp_get_initial_device().
template <typename T>
inline void device_to_host_copy_impl(T* src_ptr, T* dst_ptr, std::size_t count,
                                     camp::resources::Resource* res = nullptr) {
  std::size_t size = detail::get_size<T>(count);
  if (size == 0) {
    return;
  }

  int src_device = detail::get_device_id(src_ptr, res);
  int dst_device = omp_get_initial_device();

  UMPIRE_LOG(Debug, "omp_target_memcpy(dst_ptr = "
                        << static_cast<void*>(dst_ptr) << ", src_ptr = " << static_cast<const void*>(src_ptr)
                        << ", length = " << size << ", src_id = " << src_device << ", dst_id = " << dst_device);

  omp_target_memcpy(static_cast<void*>(dst_ptr), static_cast<const void*>(src_ptr), size, 0, 0, dst_device,
                    src_device);
}

// IMPORTANT: OpenMP Target Async Limitations
// ===========================================
// The async implementations below are NOT truly asynchronous. OpenMP 5.x does not
// provide a mature mechanism for non-blocking device operations with event tracking
// compatible with CAMP's resource system.
//
// Current behavior:
//   - All "async" operations execute synchronously
//   - Return a completed event immediately after operation finishes
//   - No actual overlap with host computation
//   - The camp::resources::Resource is validated and its pending work is
//     waited on before the operation runs (via wait_for_resource()); the
//     tracked device pointers use their allocation records, while external
//     device pointers use the resource's device
//
// This means:
//   - Performance: Same as synchronous operations
//   - Correctness: Safe, but no async performance benefit
//   - API: Maintains consistent interface with other platforms
//
// Future work: Consider using OpenMP tasks with dependencies for true async operations.

// Helper function for copy operations that returns an Event
template <typename T>
inline camp::resources::EventProxy<camp::resources::Resource> copy_async_impl(
    T* src_ptr, T* dst_ptr, std::size_t count, camp::resources::Resource& res) {
  // The src/dst devices for the copy are derived per-pointer, since a copy
  // may span two different devices.
  wait_for_resource(res);
  copy_impl(src_ptr, dst_ptr, count, &res);
  return camp::resources::EventProxy<camp::resources::Resource>{res};
}

// Helper function for host_to_device async copy
template <typename T>
inline camp::resources::EventProxy<camp::resources::Resource> host_to_device_async_impl(
    T* src_ptr, T* dst_ptr, std::size_t count, camp::resources::Resource& res) {
  wait_for_resource(res);
  host_to_device_copy_impl(src_ptr, dst_ptr, count, &res);
  return camp::resources::EventProxy<camp::resources::Resource>{res};
}

// Helper function for device_to_host async copy
template <typename T>
inline camp::resources::EventProxy<camp::resources::Resource> device_to_host_async_impl(
    T* src_ptr, T* dst_ptr, std::size_t count, camp::resources::Resource& res) {
  wait_for_resource(res);
  device_to_host_copy_impl(src_ptr, dst_ptr, count, &res);
  return camp::resources::EventProxy<camp::resources::Resource>{res};
}

// Helper function for memset operations
//
// Use the allocation's device, or the resource's device for external memory.
template <typename T>
inline void memset_impl(T* ptr, int val, std::size_t count,
                        camp::resources::Resource* res = nullptr) {
  std::size_t size = detail::get_size<T>(count);
  if (size == 0) {
    return;
  }

  int device = detail::get_device_id(ptr, res);
  unsigned char* data_ptr = reinterpret_cast<unsigned char*>(ptr);

#pragma omp target is_device_ptr(data_ptr) device(device)
#pragma omp teams distribute parallel for schedule(static, 1)
  for (std::size_t i = 0; i < size; ++i) {
    data_ptr[i] = static_cast<unsigned char>(val);
  }
}

// Helper function for memset operations that returns an Event
//
// Like memset_impl, tracked pointers retain the allocation's device regardless
// of res's device; external pointers use res's device.
template <typename T>
inline camp::resources::EventProxy<camp::resources::Resource> memset_async_impl(
    T* ptr, int val, std::size_t count, camp::resources::Resource& res) {
  wait_for_resource(res);
  memset_impl(ptr, val, count, &res);

  return camp::resources::EventProxy<camp::resources::Resource>{res};
}

// Helper function for typed (element-wise) device_memset
//
// The device id is derived from the pointer's Umpire allocation record, as
// in memset_impl.
template <typename T>
inline void device_memset_impl(T* ptr, T value, std::size_t len,
                               camp::resources::Resource* res = nullptr) {
  if (!ptr || len == 0) {
    return;
  }

  int device = detail::get_device_id(ptr, res);

  #pragma omp target teams distribute parallel for is_device_ptr(ptr) device(device)
  for (std::size_t i = 0; i < len; i++) {
    ptr[i] = value;
  }
}
} // namespace omp_target_impl
} // namespace detail

// Device-to-device copy specialization
template<>
struct copy<resource::omp_target_platform, resource::omp_target_platform> {
  template <typename T>
  static void exec(T* src_ptr, T* dst_ptr, std::size_t len) {
    detail::omp_target_impl::copy_impl(src_ptr, dst_ptr, len);
  }
  
  template <typename T>
  static camp::resources::EventProxy<camp::resources::Resource> exec(
      T* src_ptr, T* dst_ptr, std::size_t len, camp::resources::Resource& res) {
    return detail::omp_target_impl::copy_async_impl(src_ptr, dst_ptr, len, res);
  }
};

// Host-to-device copy specialization
template<>
struct copy<resource::host_platform, resource::omp_target_platform> {
  template <typename T>
  static void exec(T* src_ptr, T* dst_ptr, std::size_t len) {
    detail::omp_target_impl::host_to_device_copy_impl(src_ptr, dst_ptr, len);
  }
  
  template <typename T>
  static camp::resources::EventProxy<camp::resources::Resource> exec(
      T* src_ptr, T* dst_ptr, std::size_t len, camp::resources::Resource& res) {
    return detail::omp_target_impl::host_to_device_async_impl(src_ptr, dst_ptr, len, res);
  }
};

// Device-to-host copy specialization
template<>
struct copy<resource::omp_target_platform, resource::host_platform> {
  template <typename T>
  static void exec(T* src_ptr, T* dst_ptr, std::size_t len) {
    detail::omp_target_impl::device_to_host_copy_impl(src_ptr, dst_ptr, len);
  }
  
  template <typename T>
  static camp::resources::EventProxy<camp::resources::Resource> exec(
      T* src_ptr, T* dst_ptr, std::size_t len, camp::resources::Resource& res) {
    return detail::omp_target_impl::device_to_host_async_impl(src_ptr, dst_ptr, len, res);
  }
};

// Memset specialization
template<>
struct memset<resource::omp_target_platform> {
  template <typename T>
  static void exec(T* ptr, int val, std::size_t len) {
    detail::omp_target_impl::memset_impl(ptr, val, len);
  }

  template <typename T>
  static camp::resources::EventProxy<camp::resources::Resource> exec(
      T* ptr, int val, std::size_t len, camp::resources::Resource& res) {
    return detail::omp_target_impl::memset_async_impl(ptr, val, len, res);
  }
};

// device_memset specialization
//
// The value type V may differ from the element type T (e.g. an int literal
// for a double* buffer), so it is converted to T before launching the kernel.
template<>
struct device_memset<resource::omp_target_platform> {
  // The target device is derived from the pointer's Umpire allocation record
  // (as memset_impl() does), so the kernel runs on the device that owns the
  // memory rather than the OpenMP default device.
  template <typename T, typename V>
  static void exec(T* ptr, V val, std::size_t len) {
    detail::omp_target_impl::device_memset_impl(ptr, static_cast<T>(val), len);
  }

  // Async version: waits on the resource's pending work, then runs
  // synchronously on the pointer's device (see "OpenMP Target Async
  // Limitations" above).
  template <typename T, typename V>
  static camp::resources::EventProxy<camp::resources::Resource> exec(
      T* ptr, V val, std::size_t len, camp::resources::Resource& res) {
    detail::omp_target_impl::wait_for_resource(res);
    detail::omp_target_impl::device_memset_impl(ptr, static_cast<T>(val), len, &res);
    return camp::resources::EventProxy<camp::resources::Resource>{res};
  }
};

// Prefetch specialization
template<>
struct prefetch<resource::omp_target_platform> {
  template <typename T>
  static void exec(T* /*ptr*/, int /*device*/, std::size_t /*len*/) {
    // No-op: OpenMP Target doesn't provide explicit prefetch support
    // Data movement is handled automatically by the runtime
  }

  // Async version (also no-op)
  template <typename T>
  static camp::resources::EventProxy<camp::resources::Resource> exec(
      T* /*ptr*/, int /*device*/, std::size_t /*len*/, camp::resources::Resource& res) {
    // No-op: OpenMP Target doesn't provide explicit prefetch support
    return camp::resources::EventProxy<camp::resources::Resource>{res};
  }
};

// Note: OpenMP Target platform uses the generic reallocate implementation from operations.hpp
// since direct OpenMP Target reallocation isn't supported and memory pools require a safe allocate-copy-free pattern

} // end of namespace op
} // end of namespace umpire

#endif // UMPIRE_ENABLE_OPENMP_TARGET
