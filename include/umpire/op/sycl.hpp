//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-25, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#pragma once

#include "umpire/config.hpp"

#if defined(UMPIRE_ENABLE_SYCL)

#include "umpire/util/Platform.hpp"
#include "umpire/util/Macros.hpp"
#include "umpire/util/error.hpp"
#include "umpire/util/sycl_compat.hpp"
#include "umpire/resource/platform.hpp"
#include "umpire/op/operations.hpp"
#include "umpire/op/detail/utils.hpp"
#include "umpire/ResourceManager.hpp"
#include "camp/resource.hpp"
#include "camp/resource/event.hpp"

#include <iostream>
#include <memory>
#include <sstream>

namespace umpire {
namespace op {

// SYCL helper functions
namespace detail {

/**
 * @brief Get SYCL queue from a resource
 *
 * @param resource The resource to get the queue from
 * @return sycl::queue& The SYCL queue
 */
inline sycl::queue& get_queue(camp::resources::Resource& resource)
{
  auto sycl_resource = resource.try_get<camp::resources::Sycl>();
  if (!sycl_resource) {
    UMPIRE_ERROR(resource_error,
                 fmt::format("Expected resources::Sycl, got resources::{}",
                            platform_to_string(resource.get_platform())));
  }
  return sycl_resource->get_queue();
}

/**
 * @brief Get the SYCL queue bound to the allocation that owns ptr
 *
 * The synchronous op entry points (op::copy<sycl,sycl>::exec, op::memset<sycl>::exec,
 * etc.) are not given a camp::resources::Resource, so there is no queue to pull from a
 * resource context. Instead, mirror the pattern used by the old SyclCopyOperation /
 * SyclMemsetOperation: look the pointer up in the ResourceManager's allocation map and
 * use the sycl::queue* that was bound to that allocation's strategy via
 * MemoryResourceTraits (see umpire/strategy/AllocationStrategy.hpp getTraits() and
 * umpire/util/MemoryResourceTraits.hpp). This ensures the operation targets the same
 * device/queue the memory was allocated on, rather than a throwaway default queue.
 *
 * @param ptr Pointer previously allocated by Umpire
 * @return sycl::queue& The SYCL queue bound to the allocation's strategy
 */
inline sycl::queue& get_queue_for_ptr(const void* ptr)
{
  auto* record = ResourceManager::getInstance().findAllocationRecord(const_cast<void*>(ptr));
  auto* queue = record->strategy->getTraits().queue;
  if (!queue) {
    UMPIRE_ERROR(resource_error,
                 fmt::format("No SYCL queue bound to allocation strategy for ptr={}", fmt::ptr(ptr)));
  }
  return *queue;
}

} // namespace detail

// SYCL implementation helpers
//
// These live in a backend-specific namespace (rather than an anonymous one in
// umpire::op) so they cannot collide with the identically-shaped helpers in
// the other backend headers (e.g. op/openmp_target.hpp) when several backends
// are enabled in the same build.
namespace detail {
namespace sycl_impl {
// Error handling for SYCL operations
// Note: the caught exception must not be named `e` -- UMPIRE_ERROR declares a
// local `e` internally, and the message expression would self-reference it.
inline void sycl_error_check(sycl::event event, const char* message) {
  try {
    event.wait_and_throw();
  } catch (const sycl::exception& ex) {
    UMPIRE_ERROR(runtime_error, message + std::string(": ") + std::string(ex.what()));
  }
}

// Prepare to run an async op on an allocation-bound queue
//
// USM pointers are only valid in the sycl::context they were allocated in, so
// the async ops below run on the queue bound to the allocation (see
// detail::get_queue_for_ptr) rather than on `res`'s queue, which may belong to
// a different context. Since the op then does not run on `res`'s in-order
// queue, first validate `res` and wait on its pending work so the op is
// ordered after it (matching the old MemoryOperation::transform_async /
// apply_async pattern).
inline void wait_for_resource(camp::resources::Resource& res) {
  detail::get_queue(res);
  res.get_event().wait();
}

// Synchronous copy implementation
//
// Note: the sync copy entry points (op::copy<...>::exec without a Resource) are not
// given a camp::resources::Resource or a util::AllocationRecord*, only raw pointers.
// To source the correct sycl::queue for the device that owns the memory (rather than a
// throwaway default-constructed queue, which would target the wrong device on
// multi-device systems), we look up the queue via the allocation record bound to
// `queue_ptr` -- the pointer on the SYCL-device side of the copy -- using the same
// AllocationStrategy::getTraits().queue mechanism the old Sycl*Operation classes used.
template <typename T>
inline void copy_impl(T* src_ptr, T* dst_ptr, std::size_t count, const void* queue_ptr) {
  std::size_t size = detail::get_size<T>(count);

  sycl::queue& queue = detail::get_queue_for_ptr(queue_ptr);
  auto event = queue.memcpy(dst_ptr, src_ptr, size);
  sycl_error_check(event, "SYCL memcpy failed");
}

// Asynchronous copy implementation
//
// As with copy_impl, the copy runs on the queue bound to the allocation that
// owns `queue_ptr` (the SYCL-device side of the copy), not on `res`'s queue.
template <typename T>
inline camp::resources::EventProxy<camp::resources::Resource> copy_async_impl(
    T* src_ptr, T* dst_ptr, std::size_t count, camp::resources::Resource& res,
    const void* queue_ptr) {
  wait_for_resource(res);
  // Block until complete: the work is not on `res`'s queue, so the returned
  // EventProxy (which tracks `res`) could not otherwise represent it.
  copy_impl(src_ptr, dst_ptr, count, queue_ptr);

  return camp::resources::EventProxy<camp::resources::Resource>{res};
}

// Synchronous memset implementation
//
// As with copy_impl, no Resource/AllocationRecord is available here, only the
// pointer being memset. Source the queue from the allocation's bound queue
// (allocation->strategy->getTraits().queue), matching the old
// SyclMemsetOperation, rather than a throwaway default-constructed queue.
template <typename T>
inline void memset_impl(T* ptr, int val, std::size_t count) {
  std::size_t size = detail::get_size<T>(count);

  sycl::queue& queue = detail::get_queue_for_ptr(ptr);
  auto event = queue.memset(ptr, val, size);
  sycl_error_check(event, "SYCL memset failed");
}

// Asynchronous memset implementation (runs on the allocation-bound queue)
template <typename T>
inline camp::resources::EventProxy<camp::resources::Resource> memset_async_impl(
    T* ptr, int val, std::size_t count, camp::resources::Resource& res) {
  wait_for_resource(res);
  // Block until complete: the EventProxy tracks `res`, not the allocation's queue.
  memset_impl(ptr, val, count);

  return camp::resources::EventProxy<camp::resources::Resource>{res};
}

// Asynchronous prefetch implementation
//
// Note: `device` is accepted for interface symmetry with the other platforms'
// prefetch signatures, but is not used to select a target device here -- the
// prefetch is issued on the queue bound to `ptr`'s allocation (matching the
// old SyclMemPrefetchOperation), so it targets that queue's device. Unlike
// CUDA/HIP, SYCL's queue::prefetch() does not take a device argument; the
// queue itself is already bound to a specific device.
template <typename T>
inline camp::resources::EventProxy<camp::resources::Resource> prefetch_async_impl(
    T* ptr, int UMPIRE_UNUSED_ARG(device), std::size_t count, camp::resources::Resource& res) {

  std::size_t size = detail::get_size<T>(count);
  wait_for_resource(res);
  sycl::queue& queue = detail::get_queue_for_ptr(ptr);
  // Block until complete: the EventProxy tracks `res`, not the allocation's queue.
  sycl_error_check(queue.prefetch(ptr, size), "SYCL prefetch failed");

  return camp::resources::EventProxy<camp::resources::Resource>{res};
}
} // namespace sycl_impl
} // namespace detail

// Device-to-device copy specialization
template<>
struct copy<resource::sycl_platform, resource::sycl_platform> {
  template <typename T>
  static void exec(T* src_ptr, T* dst_ptr, std::size_t len) {
    // Both pointers are SYCL-device allocations; use the destination's bound
    // queue, matching the old SyclCopyOperation's use of dst_allocation's queue.
    detail::sycl_impl::copy_impl(src_ptr, dst_ptr, len, dst_ptr);
  }

  template <typename T>
  static camp::resources::EventProxy<camp::resources::Resource> exec(
      T* src_ptr, T* dst_ptr, std::size_t len, camp::resources::Resource& res) {
    // Use the destination's bound queue, as in the sync exec above.
    return detail::sycl_impl::copy_async_impl(src_ptr, dst_ptr, len, res, dst_ptr);
  }
};

// Host-to-device copy specialization
template<>
struct copy<resource::host_platform, resource::sycl_platform> {
  template <typename T>
  static void exec(T* src_ptr, T* dst_ptr, std::size_t len) {
    // dst_ptr is the SYCL-device allocation; use its bound queue, matching the
    // old SyclCopyToOperation's use of dst_allocation's queue.
    detail::sycl_impl::copy_impl(src_ptr, dst_ptr, len, dst_ptr);
  }

  template <typename T>
  static camp::resources::EventProxy<camp::resources::Resource> exec(
      T* src_ptr, T* dst_ptr, std::size_t len, camp::resources::Resource& res) {
    // dst_ptr is the SYCL-device allocation (see sync exec above).
    return detail::sycl_impl::copy_async_impl(src_ptr, dst_ptr, len, res, dst_ptr);
  }
};

// Device-to-host copy specialization
template<>
struct copy<resource::sycl_platform, resource::host_platform> {
  template <typename T>
  static void exec(T* src_ptr, T* dst_ptr, std::size_t len) {
    // src_ptr is the SYCL-device allocation; use its bound queue, matching the
    // old SyclCopyFromOperation's use of src_allocation's queue.
    detail::sycl_impl::copy_impl(src_ptr, dst_ptr, len, src_ptr);
  }

  template <typename T>
  static camp::resources::EventProxy<camp::resources::Resource> exec(
      T* src_ptr, T* dst_ptr, std::size_t len, camp::resources::Resource& res) {
    // src_ptr is the SYCL-device allocation (see sync exec above).
    return detail::sycl_impl::copy_async_impl(src_ptr, dst_ptr, len, res, src_ptr);
  }
};

// Memset specialization
template<>
struct memset<resource::sycl_platform> {
  template <typename T>
  static void exec(T* ptr, int val, std::size_t len) {
    detail::sycl_impl::memset_impl(ptr, val, len);
  }
  
  template <typename T>
  static camp::resources::EventProxy<camp::resources::Resource> exec(
      T* ptr, int val, std::size_t len, camp::resources::Resource& res) {
    return detail::sycl_impl::memset_async_impl(ptr, val, len, res);
  }
};

// Prefetch specialization
template<>
struct prefetch<resource::sycl_platform> {
  template <typename T>
  static void exec(T* /*ptr*/, int /*device*/, std::size_t /*len*/) {
    // SYCL prefetch requires a resource context to execute correctly. Proper
    // validation -- confirming the target device supports USM shared
    // allocations and that the pointer is actually USM-shared memory, as the
    // old SyclMemPrefetchOperation did via
    // getTraits().queue->get_device().get_info<usm_shared_allocations>() and
    // get_pointer_type() -- requires the allocation's bound sycl::queue, which
    // this bare (ptr, device, len) signature cannot supply on its own in a way
    // that is guaranteed consistent with `device`. Silently prefetching on a
    // queue chosen by other means risks issuing the prefetch on the wrong
    // device. Rather than guess, redirect callers to the async overload,
    // mirroring the precedent set by device_memset<sycl_platform>::exec above.
    UMPIRE_ERROR(runtime_error,
                 "prefetch for SYCL requires resource context. "
                 "Use the async version: prefetch(ptr, device, len, resource)");
  }

  template <typename T>
  static camp::resources::EventProxy<camp::resources::Resource> exec(
      T* ptr, int device, std::size_t len, camp::resources::Resource& res) {
    return detail::sycl_impl::prefetch_async_impl(ptr, device, len, res);
  }
};

// Forward declaration for device_memset
namespace detail {
template <typename T>
void device_memset_sycl(T* ptr, T value, std::size_t count, sycl::queue& queue);
}

// device_memset specialization
template<>
struct device_memset<resource::sycl_platform> {
  template <typename T, typename V>
  static void exec(T* /*ptr*/, V /*val*/, std::size_t /*len*/) {
    // SYCL device_memset requires a queue context to execute properly.
    // Without a resource parameter, we cannot determine which device/queue to use.
    // Use the async version with a Resource parameter instead.
    UMPIRE_ERROR(runtime_error,
                 "device_memset for SYCL requires resource context. "
                 "Use the async version: device_memset(ptr, val, len, resource)");
  }

  // Async version with resource
  //
  // The value type V may differ from the element type T (e.g. an int literal
  // for a double* buffer), so it is converted to T before launching the kernel.
  // The kernel runs on the queue bound to ptr's allocation (USM pointers are
  // only valid in their allocating context), after waiting on res's pending
  // work; device_memset_sycl blocks until completion, so the returned
  // EventProxy (which tracks res) represents completed work.
  template <typename T, typename V>
  static camp::resources::EventProxy<camp::resources::Resource> exec(
      T* ptr, V val, std::size_t len, camp::resources::Resource& res) {
    detail::sycl_impl::wait_for_resource(res);
    if (ptr && len > 0) {
      sycl::queue& queue = detail::get_queue_for_ptr(ptr);
      detail::device_memset_sycl(ptr, static_cast<T>(val), len, queue);
    }
    return camp::resources::EventProxy<camp::resources::Resource>{res};
  }
};

// Note: SYCL platform uses the generic reallocate implementation from operations.hpp
// since direct SYCL reallocation isn't supported and memory pools require a safe allocate-copy-free pattern

} // end of namespace op
} // end of namespace umpire

#endif // UMPIRE_ENABLE_SYCL