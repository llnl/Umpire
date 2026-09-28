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

} // namespace detail

// OpenMP Target implementation helpers
namespace {
// Helper function for device-to-device copy operations
//
// Both pointers live on OpenMP target devices, so the device id for each
// side is looked up independently via the Umpire allocation map (mirrors
// OpenMPTargetCopyOperation::transform, which used
// src_allocation->strategy->getTraits().id / dst_allocation->strategy->getTraits().id).
template <typename T>
inline void copy_impl(T* src_ptr, T* dst_ptr, std::size_t count) {
  std::size_t size = detail::get_size<T>(count);

  int src_device = detail::get_device_id(src_ptr);
  int dst_device = detail::get_device_id(dst_ptr);

  UMPIRE_LOG(Debug, "omp_target_memcpy(dst_ptr = "
                        << static_cast<void*>(dst_ptr) << ", src_ptr = " << static_cast<void*>(src_ptr)
                        << ", length = " << size << ", src_id = " << src_device << ", dst_id = " << dst_device);

  omp_target_memcpy(static_cast<void*>(dst_ptr), static_cast<const void*>(src_ptr), size, 0, 0, dst_device,
                    src_device);
}

// Helper function for host-to-device copy operations
//
// The host side always targets omp_get_initial_device() (mirrors the legacy
// OpenMPTargetCopyOperation, where host allocations carry
// getTraits().id == omp_get_initial_device()); the device side's id is
// looked up via the Umpire allocation map.
template <typename T>
inline void host_to_device_copy_impl(T* src_ptr, T* dst_ptr, std::size_t count) {
  std::size_t size = detail::get_size<T>(count);

  int src_device = omp_get_initial_device();
  int dst_device = detail::get_device_id(dst_ptr);

  UMPIRE_LOG(Debug, "omp_target_memcpy(dst_ptr = "
                        << static_cast<void*>(dst_ptr) << ", src_ptr = " << static_cast<const void*>(src_ptr)
                        << ", length = " << size << ", src_id = " << src_device << ", dst_id = " << dst_device);

  omp_target_memcpy(static_cast<void*>(dst_ptr), static_cast<const void*>(src_ptr), size, 0, 0, dst_device,
                    src_device);
}

// Helper function for device-to-host copy operations
//
// Mirror of host_to_device_copy_impl: the device side's id comes from the
// Umpire allocation map, the host side always targets
// omp_get_initial_device().
template <typename T>
inline void device_to_host_copy_impl(T* src_ptr, T* dst_ptr, std::size_t count) {
  std::size_t size = detail::get_size<T>(count);

  int src_device = detail::get_device_id(src_ptr);
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
//   - The camp::resources::Resource is still validated/used to target the
//     correct OpenMP device explicitly (via detail::get_device()), even
//     though the operation itself does not run concurrently with the host
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
  // Validate that res is actually an Omp resource (fail fast on mismatched
  // resource types, matching the get_stream()/get_queue() pattern used by
  // other backends). The actual src/dst devices for the copy are still
  // derived per-pointer, since a copy may span two different devices.
  detail::get_device(res);
  copy_impl(src_ptr, dst_ptr, count);
  return camp::resources::EventProxy<camp::resources::Resource>{res};
}

// Helper function for host_to_device async copy
template <typename T>
inline camp::resources::EventProxy<camp::resources::Resource> host_to_device_async_impl(
    T* src_ptr, T* dst_ptr, std::size_t count, camp::resources::Resource& res) {
  // See copy_async_impl: validate resource type, devices derived per-pointer.
  detail::get_device(res);
  host_to_device_copy_impl(src_ptr, dst_ptr, count);
  return camp::resources::EventProxy<camp::resources::Resource>{res};
}

// Helper function for device_to_host async copy
template <typename T>
inline camp::resources::EventProxy<camp::resources::Resource> device_to_host_async_impl(
    T* src_ptr, T* dst_ptr, std::size_t count, camp::resources::Resource& res) {
  // See copy_async_impl: validate resource type, devices derived per-pointer.
  detail::get_device(res);
  device_to_host_copy_impl(src_ptr, dst_ptr, count);
  return camp::resources::EventProxy<camp::resources::Resource>{res};
}

// Helper function for memset operations
//
// Mirrors OpenMPTargetMemsetOperation::apply's
// `#pragma omp target is_device_ptr(data_ptr) device(device)` +
// teams distribute parallel for pattern, with the device id derived from the
// pointer's Umpire allocation record.
template <typename T>
inline void memset_impl(T* ptr, int val, std::size_t count) {
  std::size_t size = detail::get_size<T>(count);

  int device = detail::get_device_id(ptr);
  unsigned char* data_ptr = reinterpret_cast<unsigned char*>(ptr);

#pragma omp target is_device_ptr(data_ptr) device(device)
#pragma omp teams distribute parallel for schedule(static, 1)
  for (std::size_t i = 0; i < size; ++i) {
    data_ptr[i] = static_cast<unsigned char>(val);
  }
}

// Helper function for memset operations that returns an Event
//
// Unlike memset_impl (which derives the device from the pointer's
// allocation record), the async version targets the device carried by the
// Resource explicitly, mirroring how CUDA/HIP/SYCL async ops pull their
// stream/queue from the Resource.
template <typename T>
inline camp::resources::EventProxy<camp::resources::Resource> memset_async_impl(
    T* ptr, int val, std::size_t count, camp::resources::Resource& res) {
  std::size_t size = detail::get_size<T>(count);

  int device = detail::get_device(res);
  unsigned char* data_ptr = reinterpret_cast<unsigned char*>(ptr);

#pragma omp target is_device_ptr(data_ptr) device(device)
#pragma omp teams distribute parallel for schedule(static, 1)
  for (std::size_t i = 0; i < size; ++i) {
    data_ptr[i] = static_cast<unsigned char>(val);
  }

  return camp::resources::EventProxy<camp::resources::Resource>{res};
}
} // namespace

// Device-to-device copy specialization
template<>
struct copy<resource::omp_target_platform, resource::omp_target_platform> {
  template <typename T>
  static void exec(T* src_ptr, T* dst_ptr, std::size_t len) {
    copy_impl(src_ptr, dst_ptr, len);
  }
  
  template <typename T>
  static camp::resources::EventProxy<camp::resources::Resource> exec(
      T* src_ptr, T* dst_ptr, std::size_t len, camp::resources::Resource& res) {
    return copy_async_impl(src_ptr, dst_ptr, len, res);
  }
};

// Host-to-device copy specialization
template<>
struct copy<resource::host_platform, resource::omp_target_platform> {
  template <typename T>
  static void exec(T* src_ptr, T* dst_ptr, std::size_t len) {
    host_to_device_copy_impl(src_ptr, dst_ptr, len);
  }
  
  template <typename T>
  static camp::resources::EventProxy<camp::resources::Resource> exec(
      T* src_ptr, T* dst_ptr, std::size_t len, camp::resources::Resource& res) {
    return host_to_device_async_impl(src_ptr, dst_ptr, len, res);
  }
};

// Device-to-host copy specialization
template<>
struct copy<resource::omp_target_platform, resource::host_platform> {
  template <typename T>
  static void exec(T* src_ptr, T* dst_ptr, std::size_t len) {
    device_to_host_copy_impl(src_ptr, dst_ptr, len);
  }
  
  template <typename T>
  static camp::resources::EventProxy<camp::resources::Resource> exec(
      T* src_ptr, T* dst_ptr, std::size_t len, camp::resources::Resource& res) {
    return device_to_host_async_impl(src_ptr, dst_ptr, len, res);
  }
};

// Memset specialization
template<>
struct memset<resource::omp_target_platform> {
  template <typename T>
  static void exec(T* ptr, int val, std::size_t len) {
    memset_impl(ptr, val, len);
  }

  template <typename T>
  static camp::resources::EventProxy<camp::resources::Resource> exec(
      T* ptr, int val, std::size_t len, camp::resources::Resource& res) {
    return memset_async_impl(ptr, val, len, res);
  }
};

// device_memset specialization
template<>
struct device_memset<resource::omp_target_platform> {
  // NOTE: this synchronous signature carries no device/resource information,
  // and (via the Platform-by-Value API) the pointer is not guaranteed to be
  // tracked in Umpire's allocation map, so we cannot look up a device id the
  // way memset_impl() does. This targets the OpenMP *current default
  // device* (i.e. the implicit device selected by omp_set_default_device()/
  // OMP_DEFAULT_DEVICE), matching the runtime's default `omp target` device
  // selection rules.
  template <typename T>
  static void exec(T* ptr, T val, std::size_t len) {
    if (!ptr || len == 0) {
      return;
    }

    // OpenMP target parallel loop (implicit default device)
    #pragma omp target teams distribute parallel for is_device_ptr(ptr)
    for (std::size_t i = 0; i < len; i++) {
      ptr[i] = val;
    }
  }

  // Async version: unlike the synchronous overload above, a Resource *is*
  // available here, so target its device explicitly (mirrors
  // OpenMPTargetMemsetOperation's device(...) clause).
  template <typename T>
  static camp::resources::EventProxy<camp::resources::Resource> exec(
      T* ptr, T val, std::size_t len, camp::resources::Resource& res) {
    int device = detail::get_device(res);

    if (ptr && len > 0) {
      #pragma omp target teams distribute parallel for is_device_ptr(ptr) device(device)
      for (std::size_t i = 0; i < len; i++) {
        ptr[i] = val;
      }
    }

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