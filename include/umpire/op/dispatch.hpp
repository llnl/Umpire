#pragma once

#include <limits>
#include <type_traits>

#include "umpire/ResourceManager.hpp"
#include "umpire/config.hpp"
#include "umpire/event/event.hpp"
#include "umpire/op/detail/traits.hpp"
#include "umpire/op/detail/utils.hpp"
#include "umpire/op/operations.hpp"
#include "umpire/resource/platform.hpp"

namespace umpire {
namespace op {
namespace detail {

/**
 * @brief Dispatch an operation to the appropriate platform implementation
 *
 * @tparam Op The operation template to dispatch
 * @tparam Args Argument types for the operation
 * @param platform The platform to dispatch to
 * @param args Arguments for the operation
 * @return Result of the operation
 */
template <template <typename...> class Op, typename... Args>
inline auto dispatch(camp::resources::Platform platform, Args&&... args)
{
  switch (platform) {
    case camp::resources::Platform::host:
      return Op<resource::host_platform>::exec(std::forward<Args>(args)...);
#if defined(UMPIRE_ENABLE_CUDA)
    case camp::resources::Platform::cuda:
      return Op<resource::cuda_platform>::exec(std::forward<Args>(args)...);
#endif
#if defined(UMPIRE_ENABLE_HIP)
    case camp::resources::Platform::hip:
      return Op<resource::hip_platform>::exec(std::forward<Args>(args)...);
#endif
#if defined(UMPIRE_ENABLE_SYCL)
    case camp::resources::Platform::sycl:
      return Op<resource::sycl_platform>::exec(std::forward<Args>(args)...);
#endif
#if defined(UMPIRE_ENABLE_OPENMP_TARGET)
    case camp::resources::Platform::omp_target:
      return Op<resource::omp_target_platform>::exec(std::forward<Args>(args)...);
#endif
    default:
      UMPIRE_ERROR(runtime_error, "Unknown platform for operation");
  }
}

/**
 * @brief Dispatch an operation between two platforms (same or different)
 *
 * @tparam Op The operation template to dispatch
 * @tparam Args Argument types for the operation
 * @param src_platform The source platform
 * @param dst_platform The destination platform
 * @param args Arguments for the operation
 * @return Result of the operation
 */
template <template <typename...> class Op, typename... Args>
inline auto dispatch(camp::resources::Platform src_platform, camp::resources::Platform dst_platform, Args&&... args)
{
  // Same-platform operations
  if (src_platform == dst_platform) {
    switch (src_platform) {
      case camp::resources::Platform::host:
        return Op<resource::host_platform, resource::host_platform>::exec(std::forward<Args>(args)...);
#if defined(UMPIRE_ENABLE_CUDA)
      case camp::resources::Platform::cuda:
        return Op<resource::cuda_platform, resource::cuda_platform>::exec(std::forward<Args>(args)...);
#endif
#if defined(UMPIRE_ENABLE_HIP)
      case camp::resources::Platform::hip:
        return Op<resource::hip_platform, resource::hip_platform>::exec(std::forward<Args>(args)...);
#endif
#if defined(UMPIRE_ENABLE_SYCL)
      case camp::resources::Platform::sycl:
        return Op<resource::sycl_platform, resource::sycl_platform>::exec(std::forward<Args>(args)...);
#endif
#if defined(UMPIRE_ENABLE_OPENMP_TARGET)
      case camp::resources::Platform::omp_target:
        return Op<resource::omp_target_platform, resource::omp_target_platform>::exec(
            std::forward<Args>(args)...);
#endif
      default:
        UMPIRE_ERROR(runtime_error, "Unknown platform for same-platform operation");
    }
  }

  // Cross-platform operations
#if defined(UMPIRE_ENABLE_CUDA)
  if (src_platform == camp::resources::Platform::host && dst_platform == camp::resources::Platform::cuda) {
    return Op<resource::host_platform, resource::cuda_platform>::exec(std::forward<Args>(args)...);
  }
  if (src_platform == camp::resources::Platform::cuda && dst_platform == camp::resources::Platform::host) {
    return Op<resource::cuda_platform, resource::host_platform>::exec(std::forward<Args>(args)...);
  }
#endif

#if defined(UMPIRE_ENABLE_HIP)
  if (src_platform == camp::resources::Platform::host && dst_platform == camp::resources::Platform::hip) {
    return Op<resource::host_platform, resource::hip_platform>::exec(std::forward<Args>(args)...);
  }
  if (src_platform == camp::resources::Platform::hip && dst_platform == camp::resources::Platform::host) {
    return Op<resource::hip_platform, resource::host_platform>::exec(std::forward<Args>(args)...);
  }
#endif

#if defined(UMPIRE_ENABLE_SYCL)
  if (src_platform == camp::resources::Platform::host && dst_platform == camp::resources::Platform::sycl) {
    return Op<resource::host_platform, resource::sycl_platform>::exec(std::forward<Args>(args)...);
  }
  if (src_platform == camp::resources::Platform::sycl && dst_platform == camp::resources::Platform::host) {
    return Op<resource::sycl_platform, resource::host_platform>::exec(std::forward<Args>(args)...);
  }
#endif

#if defined(UMPIRE_ENABLE_OPENMP_TARGET)
  if (src_platform == camp::resources::Platform::host && dst_platform == camp::resources::Platform::omp_target) {
    return Op<resource::host_platform, resource::omp_target_platform>::exec(std::forward<Args>(args)...);
  }
  if (src_platform == camp::resources::Platform::omp_target && dst_platform == camp::resources::Platform::host) {
    return Op<resource::omp_target_platform, resource::host_platform>::exec(std::forward<Args>(args)...);
  }
#endif

  UMPIRE_ERROR(runtime_error, "Unsupported platform combination");
}

/**
 * @brief Get the base pointer for allocation map lookup
 *
 * When given a pointer-to-pointer (T**), unwraps it to get the base pointer (T*)
 * that was originally allocated. This is necessary for looking up allocations
 * in the ResourceManager's allocation map, which tracks base pointers.
 *
 * For non-pointer types (T*), returns the pointer unchanged.
 *
 * @tparam T The type pointed to (may be a pointer type itself)
 * @param ptr The pointer to unwrap
 * @return For T**, returns *ptr (the T*). For T*, returns ptr unchanged.
 */
template <typename T>
constexpr auto get_base_ptr(T* ptr)
{
  if constexpr (std::is_pointer_v<T>) {
    return *ptr;
  } else {
    return ptr;
  }
}

// Simple RAII scope guard for cleanup on success
template<typename F>
struct scope_exit {
  F func;
  bool active = true;

  explicit scope_exit(F f) : func(std::move(f)) {}

  ~scope_exit() {
    if (active) func();
  }

  void dismiss() { active = false; }
};

template<typename F>
scope_exit<F> make_scope_exit(F f) {
  return scope_exit<F>(std::move(f));
}

} // namespace detail

// Base template for op_caller with helper functions for argument handling
template <template <typename...> class Op>
struct op_caller {
  template <typename... Args>
  static auto get_last_arg(Args... args)
  {
    return std::get<sizeof...(Args) - 1>(std::forward_as_tuple(args...));
  }

  template <size_t N, typename... Args>
  static auto get_arg(Args... args)
  {
    return std::get<N>(std::forward_as_tuple(args...));
  }

#ifdef UMPIRE_ENABLE_BOUNDS_CHECKS
  // Boundary check for memset operations
  template <typename T, typename... Args>
  static void check_memset_bounds(T* src, const util::AllocationRecord* record, std::size_t length)
  {
    std::ptrdiff_t offset = reinterpret_cast<const char*>(src) - reinterpret_cast<const char*>(record->ptr);
    if (offset < 0) {
      UMPIRE_ERROR(runtime_error, "Invalid pointer for memset bounds check");
    }

    std::size_t available_bytes = record->size - static_cast<std::size_t>(offset);

    std::size_t requested_bytes = length;
    if constexpr (!std::is_same_v<T, void>) {
      if (length > (std::numeric_limits<std::size_t>::max() / sizeof(T))) {
        UMPIRE_ERROR(runtime_error, "Requested memset size overflow");
      }
      requested_bytes = length * sizeof(T);
    }

    if (requested_bytes > 0 && requested_bytes > available_bytes) {
      UMPIRE_ERROR(runtime_error,
                   fmt::format("Cannot memset over the end of allocation: {} -> {}", requested_bytes, available_bytes));
    }
  }

  // Boundary check for copy operations
  template <typename T, typename... Args>
  static void check_copy_bounds(T* src, T* dst, const util::AllocationRecord* src_record,
                                const util::AllocationRecord* dst_record, std::size_t size)
  {
    std::ptrdiff_t src_offset = reinterpret_cast<const char*>(src) - reinterpret_cast<const char*>(src_record->ptr);
    if (src_offset < 0) {
      UMPIRE_ERROR(runtime_error, "Invalid source pointer for copy bounds check");
    }
    std::size_t src_available_bytes = src_record->size - static_cast<std::size_t>(src_offset);

    std::ptrdiff_t dst_offset = reinterpret_cast<const char*>(dst) - reinterpret_cast<const char*>(dst_record->ptr);
    if (dst_offset < 0) {
      UMPIRE_ERROR(runtime_error, "Invalid destination pointer for copy bounds check");
    }
    std::size_t dst_available_bytes = dst_record->size - static_cast<std::size_t>(dst_offset);

    std::size_t requested_bytes = size;
    if constexpr (!std::is_same_v<T, void>) {
      if (size > (std::numeric_limits<std::size_t>::max() / sizeof(T))) {
        UMPIRE_ERROR(runtime_error, "Requested copy size overflow");
      }
      requested_bytes = size * sizeof(T);
    }

    if (requested_bytes == 0) {
      return;
    }

    if (requested_bytes > src_available_bytes) {
      UMPIRE_ERROR(runtime_error,
                   fmt::format("Not enough data in source to copy {} bytes from {} bytes", requested_bytes,
                               src_available_bytes));
    }

    if (requested_bytes > dst_available_bytes) {
      UMPIRE_ERROR(runtime_error,
                   fmt::format("Not enough space in destination to copy {} bytes into {} bytes", requested_bytes,
                               dst_available_bytes));
    }
  }
#endif // UMPIRE_ENABLE_BOUNDS_CHECKS

  // Pointer used for the allocation map lookup in single-pointer operations.
  // Only reallocate takes a pointer-to-pointer in-out parameter (T**) whose
  // pointee is the allocation; every other operation operates on the pointer
  // itself, even when the element type is itself a pointer (e.g. double**).
  template <typename T>
  static auto get_lookup_ptr(T* ptr)
  {
    if constexpr (std::is_same_v<Op<resource::host_platform>, reallocate<resource::host_platform>>) {
      return detail::get_base_ptr(ptr);
    } else {
      return ptr;
    }
  }

  // memset and device_memset both take {value, length} and share bounds checks
  static constexpr bool is_memset_op()
  {
    return std::is_same_v<Op<resource::host_platform>, memset<resource::host_platform>> ||
           std::is_same_v<Op<resource::host_platform>, device_memset<resource::host_platform>>;
  }

  template <typename T, typename... Args>
  inline static auto exec(T* src, Args... args)
  {
    auto& rm = ResourceManager::getInstance();
    auto& allocation_map = rm.m_allocations;
    auto src_record = allocation_map.find(get_lookup_ptr(src));
    auto p = src_record->strategy->getPlatform();

    if constexpr (is_memset_op()) {
#ifdef UMPIRE_ENABLE_BOUNDS_CHECKS
      std::size_t length = get_arg<1>(args...);
      check_memset_bounds(src, src_record, length);
#endif
    }

    return detail::dispatch<Op>(p, src, args...);
  }

  template <typename T, typename... Args>
  inline static auto exec(T* src, Args... args, camp::resources::Resource& ctx)
  {
    auto& rm = ResourceManager::getInstance();
    auto& allocation_map = rm.m_allocations;
    auto src_record = allocation_map.find(get_lookup_ptr(src));
    auto p = src_record->strategy->getPlatform();

    if constexpr (is_memset_op()) {
#ifdef UMPIRE_ENABLE_BOUNDS_CHECKS
      std::size_t length = get_arg<1>(args...);
      check_memset_bounds(src, src_record, length);
#endif
    }

    return detail::dispatch<Op>(p, src, std::forward<Args>(args)..., ctx);
  }

  template <typename T, typename... Args>
  inline static auto exec(T* src, T* dst, Args... args)
  {
    auto& rm = ResourceManager::getInstance();
    auto& allocation_map = rm.m_allocations;
    auto src_record = allocation_map.find(src);
    auto dst_record = allocation_map.find(dst);

    auto p1 = src_record->strategy->getPlatform();
    auto p2 = dst_record->strategy->getPlatform();

    if constexpr (std::is_same_v<Op<resource::host_platform, resource::host_platform>,
                                 copy<resource::host_platform, resource::host_platform>>) {
#ifdef UMPIRE_ENABLE_BOUNDS_CHECKS
      std::size_t size = get_arg<0>(args...);
      check_copy_bounds(src, dst, src_record, dst_record, size);
#endif
    }

    return detail::dispatch<Op>(p1, p2, src, dst, args...);
  }

  template <typename T, typename... Args>
  inline static auto exec(T* src, T* dst, Args... args, camp::resources::Resource& ctx)
  {
    auto& rm = ResourceManager::getInstance();
    auto& allocation_map = rm.m_allocations;
    auto src_record = allocation_map.find(src);
    auto dst_record = allocation_map.find(dst);

    auto p1 = src_record->strategy->getPlatform();
    auto p2 = dst_record->strategy->getPlatform();

    if constexpr (std::is_same_v<Op<resource::host_platform, resource::host_platform>,
                                 copy<resource::host_platform, resource::host_platform>>) {
#ifdef UMPIRE_ENABLE_BOUNDS_CHECKS
      std::size_t size = get_arg<0>(args...);
      check_copy_bounds(src, dst, src_record, dst_record, size);
#endif
    }

    return detail::dispatch<Op>(p1, p2, src, dst, std::forward<Args>(args)..., ctx);
  }
};

namespace detail {

// Record only at public tracked-operation boundaries. Internal operations and
// explicit-platform overloads dispatch directly without producing these events.
template <typename T>
void record_copy(T* src, T* dst, std::size_t len, bool async = false)
{
  umpire::event::record([&](auto& event) {
    auto& rm = ResourceManager::getInstance();
    auto src_record = rm.findAllocationRecord(src);
    auto dst_record = rm.findAllocationRecord(dst);
    event.name("copy")
        .category(event::category::operation)
        .arg("src", static_cast<void*>(src))
        .arg("dst", static_cast<void*>(dst))
        .arg("src_offset", reinterpret_cast<char*>(src) - static_cast<char*>(src_record->ptr))
        .arg("dst_offset", reinterpret_cast<char*>(dst) - static_cast<char*>(dst_record->ptr))
        .arg("size", get_size<T>(len))
        .arg("src_allocator_ref", static_cast<void*>(src_record->strategy))
        .arg("dst_allocator_ref", static_cast<void*>(dst_record->strategy))
        .tag("src_allocator_name", src_record->strategy->getName())
        .tag("dst_allocator_name", dst_record->strategy->getName())
        .tag("replay", "true");
    if (async) {
      event.tag("async", "true");
    }
  });
}

template <typename T, typename V>
void record_memset(T* ptr, V value, std::size_t len, bool async = false)
{
  umpire::event::record([&](auto& event) {
    auto record = ResourceManager::getInstance().findAllocationRecord(ptr);
    event.name("memset")
        .category(event::category::operation)
        .arg("ptr", static_cast<void*>(ptr))
        .arg("value", static_cast<int>(value))
        .arg("size", get_size<T>(len))
        .arg("allocator_ref", static_cast<void*>(record->strategy))
        .tag("allocator_name", record->strategy->getName())
        .tag("replay", "true");
    if (async) {
      event.tag("async", "true");
    }
  });
}

template <typename T>
strategy::AllocationStrategy* record_reallocate_begin(T* ptr, std::size_t len, bool async = false)
{
  strategy::AllocationStrategy* strategy = nullptr;
  umpire::event::record([&](auto& event) {
    auto& rm = ResourceManager::getInstance();
    strategy = (ptr ? rm.getAllocator(ptr) : rm.getDefaultAllocator()).getAllocationStrategy();
    event.name("reallocate")
        .category(event::category::operation)
        .arg("current_ptr", static_cast<void*>(ptr))
        .arg("size", get_size<T>(len))
        .arg("allocator_ref", static_cast<void*>(strategy))
        .tag("allocator_name", strategy->getName())
        .tag("replay", "true");
    if (async) {
      event.tag("async", "true");
    }
  });
  return strategy;
}

template <typename T>
void record_reallocate_end(T* ptr, strategy::AllocationStrategy* strategy)
{
  umpire::event::record([&](auto& event) {
    event.name("reallocate")
        .category(event::category::operation)
        .arg("new_ptr", static_cast<void*>(ptr))
        .arg("allocator_ref", static_cast<void*>(strategy))
        .tag("allocator_name", strategy->getName());
  });
}

} // namespace detail

} // namespace op

//------------------------------------------------------------------------------
// Public API Semantics for Copy and Reallocate Operations
//------------------------------------------------------------------------------
//
// The following functions use different semantics for size parameters based on
// pointer type, matching standard C++ conventions:
//
// COPY OPERATIONS:
//   - umpire::copy(T* src, T* dst, std::size_t len) for non-void T:
//     len is a COUNT OF ELEMENTS (will be multiplied by sizeof(T) internally)
//
//   - umpire::copy(void* src, void* dst, std::size_t len):
//     len is BYTES (used directly, no sizeof multiplication)
//
// REALLOCATE OPERATIONS:
//   - umpire::reallocate(T** ptr, std::size_t new_size) for non-void T:
//     new_size is a COUNT OF ELEMENTS (will be multiplied by sizeof(T) internally)
//
//   - umpire::reallocate(void** ptr, std::size_t new_size):
//     new_size is BYTES (used directly, no sizeof multiplication)
//
// RATIONALE:
//   This matches how detail::get_size<T>(count) works:
//   - For void: returns count as-is (bytes)
//   - For typed pointers: returns count * sizeof(T) (elements to bytes)
//
// This keeps the API consistent with C++ idioms where typed operations work
// with element counts and void* operations work with byte counts.
//
// *** ARGUMENT ORDER WARNING ***
//   umpire::copy(src, dst, ...) takes (SOURCE, DESTINATION) order.
//   This is the REVERSE of the deprecated ResourceManager::copy(dst_ptr, src_ptr, ...),
//   which takes (DESTINATION, SOURCE) order.
//
//   A mechanical migration from ResourceManager::copy() to umpire::copy() that does
//   NOT swap the two pointer arguments will compile cleanly (both overloads accept
//   the same pointer type) and will silently copy data in the WRONG DIRECTION --
//   there is no compiler error or warning to catch this mistake. Callers migrating
//   from ResourceManager::copy(dst, src, ...) MUST swap to umpire::copy(src, dst, ...).
//
//   Also note the size-parameter semantics above still apply: the typed
//   umpire::copy<T>(src, dst, len) overloads take len as an ELEMENT COUNT, while the
//   void* umpire::copy(src, dst, len) overload takes len as a BYTE COUNT.
//------------------------------------------------------------------------------

// Global operation implementations that use the op_caller
//
// NOTE: Argument order is (src, dst) -- the REVERSE of the deprecated
// ResourceManager::copy(dst, src, ...). See the "ARGUMENT ORDER WARNING" above.
template <typename T>
void copy(T* src, T* dst, std::size_t len)
{
  op::detail::record_copy(src, dst, len);
  op::op_caller<op::copy>::exec(src, dst, len);
}

template <typename T>
auto copy(T* src, T* dst, std::size_t len, camp::resources::Resource& ctx)
{
  op::detail::record_copy(src, dst, len, true);
  return op::op_caller<op::copy>::exec(src, dst, len, ctx);
}

template <typename T, typename V>
void memset(T* src, V v, std::size_t len)
{
  op::detail::record_memset(src, v, len);
  op::op_caller<op::memset>::exec(src, v, len);
}

template <typename T>
camp::resources::EventProxy<camp::resources::Resource> memset(T* src, int v, std::size_t len,
                                                              camp::resources::Resource& ctx)
{
  op::detail::record_memset(src, v, len, true);
  return op::op_caller<op::memset>::exec(src, v, len, ctx);
}

template <typename T, typename V>
void device_memset(T* ptr, V v, std::size_t len)
{
  op::op_caller<op::device_memset>::exec(ptr, v, len);
}

// Asynchronous device_memset. Required for SYCL, whose synchronous
// device_memset cannot determine a queue and throws directing callers here.
template <typename T, typename V>
camp::resources::EventProxy<camp::resources::Resource> device_memset(T* ptr, V v, std::size_t len,
                                                                     camp::resources::Resource& ctx)
{
  return op::op_caller<op::device_memset>::exec(ptr, v, len, ctx);
}

template <typename T>
inline T* reallocate(T** src, std::size_t size)
{
  auto strategy = op::detail::record_reallocate_begin(*src, size);
  T* result;
  // Handle nullptr specially - op_caller can't look up null in allocation map
  if (*src == nullptr) {
    result = op::reallocate<resource::host_platform>::exec(src, size);
  } else {
    result = op::op_caller<op::reallocate>::exec(src, size);
  }
  op::detail::record_reallocate_end(result, strategy);
  return result;
}

// Async reallocate implementation
template <typename T>
inline camp::resources::EventProxy<camp::resources::Resource> reallocate(T** src, std::size_t size,
                                                                         camp::resources::Resource& ctx)
{
  auto strategy = op::detail::record_reallocate_begin(*src, size, true);
  // Handle nullptr specially - op_caller can't look up null in allocation map
  auto event = (*src == nullptr) ? op::reallocate<resource::host_platform>::exec(src, size, ctx)
                                 : op::op_caller<op::reallocate>::exec(src, size, ctx);
  op::detail::record_reallocate_end(*src, strategy);
  return event;
}

// Synchronous prefetch implementation
template <typename T>
void prefetch(T* ptr, int device, std::size_t size)
{
  op::op_caller<op::prefetch>::exec(ptr, device, size);
}

// Asynchronous prefetch implementation
template <typename T>
camp::resources::EventProxy<camp::resources::Resource> prefetch(T* ptr, int device, std::size_t size,
                                                                camp::resources::Resource& ctx)
{
  return op::op_caller<op::prefetch>::exec(ptr, device, size, ctx);
}

// Individual convenience functions with auto-dispatch
template <typename T>
void set_accessed_by(T* ptr, int device, std::size_t size)
{
  op::op_caller<op::set_accessed_by>::exec(ptr, device, size);
}

template <typename T>
void set_preferred_location(T* ptr, int device, std::size_t size)
{
  op::op_caller<op::set_preferred_location>::exec(ptr, device, size);
}

template <typename T>
void set_read_mostly(T* ptr, int device, std::size_t size)
{
  op::op_caller<op::set_read_mostly>::exec(ptr, device, size);
}

template <typename T>
void unset_accessed_by(T* ptr, int device, std::size_t size)
{
  op::op_caller<op::unset_accessed_by>::exec(ptr, device, size);
}

template <typename T>
void unset_preferred_location(T* ptr, int device, std::size_t size)
{
  op::op_caller<op::unset_preferred_location>::exec(ptr, device, size);
}

template <typename T>
void unset_read_mostly(T* ptr, int device, std::size_t size)
{
  op::op_caller<op::unset_read_mostly>::exec(ptr, device, size);
}

#if (defined(UMPIRE_ENABLE_HIP) && HIP_VERSION_MAJOR >= 5)
template <typename T>
void set_coarse_grain(T* ptr, int device, std::size_t size)
{
  op::op_caller<op::set_coarse_grain>::exec(ptr, device, size);
}

template <typename T>
void unset_coarse_grain(T* ptr, int device, std::size_t size)
{
  op::op_caller<op::unset_coarse_grain>::exec(ptr, device, size);
}
#endif

//------------------------------------------------------------------------------
// Reallocate implementations (moved from operations.hpp to avoid circular dependency)
//------------------------------------------------------------------------------

// Private synchronous implementation helper
template <typename Platform>
template <typename PtrType>
PtrType* op::reallocate<Platform>::reallocate_impl_sync(PtrType** ptr_ptr, std::size_t new_size)
{
  PtrType* current_ptr = *ptr_ptr;

  if (!current_ptr) {
    auto& rm = ResourceManager::getInstance();
    Allocator allocator = rm.getDefaultAllocator();

    std::size_t alloc_bytes = detail::get_size<PtrType>(new_size);

    PtrType* new_ptr = static_cast<PtrType*>(allocator.allocate(alloc_bytes));
    *ptr_ptr = new_ptr;
    return new_ptr;
  }

  auto& rm = ResourceManager::getInstance();
  auto& allocation_map = rm.m_allocations;
  Allocator allocator = rm.getAllocator(current_ptr);

  auto alloc_record = allocation_map.find(current_ptr);
  if (current_ptr != alloc_record->ptr) {
    UMPIRE_ERROR(runtime_error, fmt::format("Cannot reallocate an offset ptr (ptr={}, base={})",
                                            reinterpret_cast<void*>(current_ptr), alloc_record->ptr));
  }

  std::size_t old_size = rm.getSize(current_ptr);
  std::size_t new_bytes = detail::get_size<PtrType>(new_size);

  if (new_bytes == 0) {
    allocator.deallocate(current_ptr);
    PtrType* new_ptr = static_cast<PtrType*>(allocator.allocate(0));
    *ptr_ptr = new_ptr;
    return new_ptr;
  }

  PtrType* new_ptr = static_cast<PtrType*>(allocator.allocate(new_bytes));

  std::size_t copy_bytes = (old_size > new_bytes) ? new_bytes : old_size;

  // Guard new pointer for exception safety
  auto new_cleanup = detail::make_scope_exit([&]() {
    allocator.deallocate(new_ptr);
  });

  op::op_caller<op::copy>::exec(static_cast<void*>(current_ptr), static_cast<void*>(new_ptr), copy_bytes);

  allocator.deallocate(current_ptr);

  new_cleanup.dismiss();
  *ptr_ptr = new_ptr;

  return new_ptr;
}

// Private asynchronous implementation helper
template <typename Platform>
template <typename PtrType>
camp::resources::EventProxy<camp::resources::Resource> op::reallocate<Platform>::reallocate_impl_async(
    PtrType** ptr_ptr, std::size_t new_size, camp::resources::Resource& ctx)
{
  PtrType* current_ptr = *ptr_ptr;

  if (!current_ptr) {
    auto& rm = ResourceManager::getInstance();
    Allocator allocator = rm.getDefaultAllocator();

    std::size_t alloc_bytes = detail::get_size<PtrType>(new_size);

    PtrType* new_ptr = static_cast<PtrType*>(allocator.allocate(alloc_bytes));
    *ptr_ptr = new_ptr;
    return camp::resources::EventProxy<camp::resources::Resource>{ctx};
  }

  auto& rm = ResourceManager::getInstance();
  auto& allocation_map = rm.m_allocations;
  Allocator allocator = rm.getAllocator(current_ptr);

  auto alloc_record = allocation_map.find(current_ptr);
  if (current_ptr != alloc_record->ptr) {
    UMPIRE_ERROR(runtime_error, fmt::format("Cannot reallocate an offset ptr (ptr={}, base={})",
                                            reinterpret_cast<void*>(current_ptr), alloc_record->ptr));
  }

  std::size_t old_size = rm.getSize(current_ptr);
  std::size_t new_bytes = detail::get_size<PtrType>(new_size);

  if (new_bytes == 0) {
    allocator.deallocate(current_ptr);
    PtrType* new_ptr = static_cast<PtrType*>(allocator.allocate(0));
    *ptr_ptr = new_ptr;
    return camp::resources::EventProxy<camp::resources::Resource>{ctx};
  }

  PtrType* new_ptr = static_cast<PtrType*>(allocator.allocate(new_bytes));

  // Guard new pointer for exception safety: if the copy below throws (or the
  // wait throws), release the new allocation so it is not leaked and *ptr_ptr
  // is not left dangling.
  auto new_cleanup = detail::make_scope_exit([&]() {
    allocator.deallocate(new_ptr);
  });

  std::size_t copy_bytes = (old_size > new_bytes) ? new_bytes : old_size;

  auto event = op::op_caller<op::copy>::exec(static_cast<void*>(current_ptr), static_cast<void*>(new_ptr), copy_bytes, ctx);

  // NOTE: This wait is an intentional blocking point. It guarantees the copy
  // has completed before the old buffer is freed (and before we dismiss the
  // new-pointer guard below). If the wait throws, only the new buffer is
  // released; the old buffer and *ptr_ptr are left untouched and valid. A true
  // non-blocking async reallocate that defers the old-buffer free until the
  // event completes is left as a follow-up.
  static_cast<camp::resources::Event>(event).wait();

  allocator.deallocate(current_ptr);

  new_cleanup.dismiss();

  *ptr_ptr = new_ptr;
  return event;
}

// Public API: Typed pointer, synchronous
template <typename Platform>
template <typename T>
T* op::reallocate<Platform>::exec(T** ptr, std::size_t new_size)
{
  return reallocate_impl_sync(ptr, new_size);
}

// Public API: Typed pointer, asynchronous
template <typename Platform>
template <typename T>
camp::resources::EventProxy<camp::resources::Resource> op::reallocate<Platform>::exec(T** ptr_ptr, std::size_t new_size,
                                                                                       camp::resources::Resource& ctx)
{
  return reallocate_impl_async(ptr_ptr, new_size, ctx);
}

// Public API: Void pointer, synchronous
template <typename Platform>
void* op::reallocate<Platform>::exec(void** ptr_ptr, std::size_t new_size)
{
  return reallocate_impl_sync(ptr_ptr, new_size);
}

// Public API: Void pointer, asynchronous
template <typename Platform>
camp::resources::EventProxy<camp::resources::Resource> op::reallocate<Platform>::exec(void** ptr_ptr, std::size_t new_size,
                                                                                       camp::resources::Resource& ctx)
{
  return reallocate_impl_async(ptr_ptr, new_size, ctx);
}

template <typename SrcPlatform, typename DstPlatform, typename T>
void copy(T* src, T* dst, std::size_t len)
{
  op::copy<SrcPlatform, DstPlatform>::exec(src, dst, len);
}

template <typename SrcPlatform, typename DstPlatform, typename T>
auto copy(T* src, T* dst, std::size_t len, camp::resources::Resource& ctx)
{
  return op::copy<SrcPlatform, DstPlatform>::exec(src, dst, len, ctx);
}

// Direct template memset functions
template <typename Platform, typename T>
void memset(T* ptr, int value, std::size_t len)
{
  op::memset<Platform>::exec(ptr, value, len);
}

template <typename Platform, typename T>
auto memset(T* ptr, int value, std::size_t len, camp::resources::Resource& ctx)
{
  return op::memset<Platform>::exec(ptr, value, len, ctx);
}

template <typename Platform, typename T, typename V>
void device_memset(T* ptr, V value, std::size_t len)
{
  op::device_memset<Platform>::exec(ptr, value, len);
}

// Direct template prefetch functions
template <typename Platform, typename T>
void prefetch(T* ptr, int device, std::size_t len)
{
  op::prefetch<Platform>::exec(ptr, device, len);
}

template <typename Platform, typename T>
auto prefetch(T* ptr, int device, std::size_t len, camp::resources::Resource& ctx)
{
  return op::prefetch<Platform>::exec(ptr, device, len, ctx);
}

template <typename Platform, typename T>
std::enable_if_t<op::detail::supports_memory_advice<Platform>::value> set_accessed_by(T* ptr, int device,
                                                                                      std::size_t len)
{
  op::set_accessed_by<Platform>::exec(ptr, device, len);
}

template <typename Platform, typename T>
std::enable_if_t<op::detail::supports_memory_advice<Platform>::value> set_preferred_location(T* ptr, int device,
                                                                                             std::size_t len)
{
  op::set_preferred_location<Platform>::exec(ptr, device, len);
}

template <typename Platform, typename T>
std::enable_if_t<op::detail::supports_memory_advice<Platform>::value> set_read_mostly(T* ptr, int device,
                                                                                      std::size_t len)
{
  op::set_read_mostly<Platform>::exec(ptr, device, len);
}

template <typename Platform, typename T>
std::enable_if_t<op::detail::supports_memory_advice<Platform>::value> unset_accessed_by(T* ptr, int device,
                                                                                        std::size_t len)
{
  op::unset_accessed_by<Platform>::exec(ptr, device, len);
}

template <typename Platform, typename T>
std::enable_if_t<op::detail::supports_memory_advice<Platform>::value> unset_preferred_location(T* ptr, int device,
                                                                                               std::size_t len)
{
  op::unset_preferred_location<Platform>::exec(ptr, device, len);
}

template <typename Platform, typename T>
std::enable_if_t<op::detail::supports_memory_advice<Platform>::value> unset_read_mostly(T* ptr, int device,
                                                                                        std::size_t len)
{
  op::unset_read_mostly<Platform>::exec(ptr, device, len);
}

#if (defined(UMPIRE_ENABLE_HIP) && HIP_VERSION_MAJOR >= 5)
template <typename Platform, typename T>
std::enable_if_t<op::detail::supports_memory_advice<Platform>::value> set_coarse_grain(T* ptr, int device,
                                                                                       std::size_t len)
{
  op::set_coarse_grain<Platform>::exec(ptr, device, len);
}

template <typename Platform, typename T>
std::enable_if_t<op::detail::supports_memory_advice<Platform>::value> unset_coarse_grain(T* ptr, int device,
                                                                                         std::size_t len)
{
  op::unset_coarse_grain<Platform>::exec(ptr, device, len);
}
#endif

//------------------------------------------------------------------------------
// Platform-by-Value API
//------------------------------------------------------------------------------
//
// These overloads take explicit camp::resources::Platform values for dispatch.
// Host/CUDA/HIP copy and memset can operate on external pointers without map
// lookups. SYCL/OpenMP use allocation records for tracked device pointers;
// external device pointers require a Resource carrying their context/device.
// Reallocate always requires a tracked pointer or nullptr.
// These overloads do not record operation events (allocation events still apply).
//
// Thread safety: safe on different pointers; concurrent operations on the same
// pointer require external synchronization.
//
// Errors: throw umpire::runtime_error for unsupported platform combinations
// (e.g. direct CUDA<->HIP copy), platform-specific operation failures, and
// resource/platform mismatches (e.g. a CUDA resource with Platform::hip).
//
// Example:
// ```cpp
// // External memory not tracked by Umpire
// float* external_gpu = /* from third-party library */;
// float* host_buffer = new float[1000];
//
// // Explicit platform specification
// umpire::copy(Platform::cuda, Platform::host,
//              external_gpu, host_buffer, 1000);
//
// // With async execution
// camp::resources::Resource cuda_ctx{camp::resources::Cuda{}};
// auto event = umpire::copy(Platform::cuda, Platform::host,
//                           external_gpu, host_buffer, 1000, cuda_ctx);
// static_cast<camp::resources::Event>(event).wait();
// ```
//------------------------------------------------------------------------------

/**
 * @brief Copy memory between two pointers with explicit platform specification
 *
 * Host/CUDA/HIP support external pointers without allocation-map lookups.
 * SYCL/OpenMP require tracked device pointers for this overload; use the
 * Resource-taking overload for external device memory.
 *
 * @tparam T Type of data being copied (affects size calculation)
 * @param src_platform Platform where source memory resides (host, cuda, hip, sycl, omp_target)
 * @param dst_platform Platform where destination memory resides
 * @param src Source pointer
 * @param dst Destination pointer
 * @param len Number of elements to copy (bytes if T=void, elements otherwise)
 *
 * **Thread Safety**: Safe to call from multiple threads on different pointers.
 * Concurrent operations on the same pointer are not thread-safe.
 *
 * **Performance**: Host/CUDA/HIP avoid allocation-map lookup overhead.
 * SYCL/OpenMP look up device pointers to select the owning queue/device.
 *
 * **Error Handling**: Throws runtime_error if platform combination is unsupported
 * or if the underlying platform operation fails.
 */
template <typename T>
void copy(camp::resources::Platform src_platform, camp::resources::Platform dst_platform, T* src, T* dst,
          std::size_t len)
{
  op::detail::dispatch<op::copy>(src_platform, dst_platform, src, dst, len);
}

/**
 * @brief Asynchronous copy with explicit platform specification
 *
 * Performs an asynchronous memory copy using the provided resource context.
 * The operation may execute concurrently with host code depending on platform
 * capabilities (asynchronous on CUDA/HIP, blocking on SYCL/OpenMP Target).
 *
 * @tparam T Type of data being copied
 * @param src_platform Platform where source memory resides
 * @param dst_platform Platform where destination memory resides
 * @param src Source pointer
 * @param dst Destination pointer
 * @param len Number of elements to copy
 * @param ctx Resource context providing stream/queue for async execution
 *
 * @return EventProxy that can be waited on for completion
 *
 * **Resource Type**: For copies involving device memory, ctx must match the
 * device backend. Host-to-host copies wait for ctx before accessing memory.
 * External SYCL device pointers must belong to the selected queue's context;
 * external OpenMP device pointers must reside on ctx's device. Tracked device
 * pointers retain their allocation-bound queue/device selection.
 *
 * **Platform-Specific Behavior**:
 * - CUDA/HIP: Asynchronous execution on the device stream
 * - SYCL/OpenMP Target: Blocking execution after prior resource work completes
 * - Host: Synchronous execution, returns completed event immediately
 */
template <typename T>
auto copy(camp::resources::Platform src_platform, camp::resources::Platform dst_platform, T* src, T* dst,
          std::size_t len, camp::resources::Resource& ctx)
{
  return op::detail::dispatch<op::copy>(src_platform, dst_platform, src, dst, len, ctx);
}

/**
 * @brief Fill memory with a byte value using explicit platform specification
 *
 * Sets each byte in the memory region to the specified value. Host/CUDA/HIP
 * support external memory without allocation-map lookups. SYCL/OpenMP require
 * a tracked device pointer; use the Resource-taking overload for external memory.
 *
 * @tparam T Type of pointer (affects size calculation)
 * @tparam V Type of value (typically int or unsigned char)
 * @param platform Platform where memory resides (host, cuda, hip, sycl, omp_target)
 * @param ptr Pointer to memory region
 * @param value Byte value to set (0-255, higher bytes ignored)
 * @param len Number of elements (bytes if T=void, elements otherwise)
 *
 * **Behavior**: Sets each byte to (value & 0xFF). For typed pointers, operates
 * on len*sizeof(T) bytes.
 *
 * **Thread Safety**: Safe to call from multiple threads on different pointers.
 */
template <typename T, typename V>
void memset(camp::resources::Platform platform, T* ptr, V value, std::size_t len)
{
  op::detail::dispatch<op::memset>(platform, ptr, value, len);
}

/**
 * @brief Asynchronous memset with explicit platform specification
 *
 * @tparam T Type of pointer
 * @param platform Platform where memory resides
 * @param ptr Pointer to memory region
 * @param value Byte value to set
 * @param len Number of elements
 * @param ctx Resource context for async execution
 *
 * @return EventProxy for completion synchronization
 *
 * **Platform-Specific Behavior**: See copy() async documentation for execution
 * and external-pointer context/device requirements.
 */
template <typename T>
camp::resources::EventProxy<camp::resources::Resource> memset(camp::resources::Platform platform, T* ptr, int value,
                                                              std::size_t len, camp::resources::Resource& ctx)
{
  return op::detail::dispatch<op::memset>(platform, ptr, value, len, ctx);
}

/**
 * @brief Set device memory to typed values using explicit platform specification
 *
 * Unlike memset which operates on bytes, device_memset sets typed values element-wise.
 * For example, device_memset<int>(ptr, 42, 100) sets 100 integers to value 42.
 *
 * @tparam T Element type (int, float, double, etc.)
 * @tparam V Value type (must be compatible with T)
 * @param platform Platform where memory resides (typically cuda, hip, or sycl)
 * @param ptr Pointer to device memory
 * @param value Value to set each element to
 * @param len Number of elements (NOT bytes)
 *
 * **Platform Support**:
 * - CUDA: Uses custom kernel, requires Resource parameter for async
 * - HIP: Uses custom kernel, requires Resource parameter for async
 * - SYCL: Uses parallel_for, ALWAYS requires Resource parameter
 * - OpenMP Target: Uses pragmas, works without Resource
 * - Host: Not supported (use memset or std::fill instead)
 *
 * **Important**: SYCL requires a Resource parameter. This overload cannot be
 * used for SYCL; use the tracked-pointer Resource-taking device_memset overload.
 * OpenMP requires a tracked pointer to identify the owning device.
 */
template <typename T, typename V>
void device_memset(camp::resources::Platform platform, T* ptr, V value, std::size_t len)
{
  op::detail::dispatch<op::device_memset>(platform, ptr, value, len);
}

namespace op {
namespace detail {

inline Allocator get_reallocate_allocator(camp::resources::Platform platform, void* ptr)
{
  const char* resource_name;
  switch (platform) {
    case camp::resources::Platform::host:
      resource_name = "HOST";
      break;
#if defined(UMPIRE_ENABLE_CUDA)
    case camp::resources::Platform::cuda:
#endif
#if defined(UMPIRE_ENABLE_HIP)
    case camp::resources::Platform::hip:
#endif
#if defined(UMPIRE_ENABLE_SYCL)
    case camp::resources::Platform::sycl:
#endif
#if defined(UMPIRE_ENABLE_OPENMP_TARGET)
    case camp::resources::Platform::omp_target:
#endif
#if defined(UMPIRE_ENABLE_CUDA) || defined(UMPIRE_ENABLE_HIP) || defined(UMPIRE_ENABLE_SYCL) || \
    defined(UMPIRE_ENABLE_OPENMP_TARGET)
      resource_name = "DEVICE";
      break;
#endif
    default:
      UMPIRE_ERROR(runtime_error, "Unsupported platform for reallocate");
  }

  auto& rm = ResourceManager::getInstance();
  auto allocator = ptr ? rm.getAllocator(ptr) : rm.getDefaultAllocator();
  if (!ptr && allocator.getPlatform() != platform) {
    allocator = rm.getAllocator(resource_name);
  }
  if (allocator.getPlatform() != platform) {
    UMPIRE_ERROR(runtime_error,
                 fmt::format("Allocator \"{}\" does not match the requested reallocate platform {}",
                             allocator.getName(), static_cast<int>(platform)));
  }
  return allocator;
}

} // namespace detail
} // namespace op

/**
 * @brief Reallocate typed pointer with explicit platform specification
 *
 * Resizes an allocation by allocating new memory, copying data, and freeing old memory.
 * Non-null pointers must be tracked by Umpire and belong to the requested platform.
 *
 * @tparam T Element type
 * @param platform Platform where memory resides
 * @param ptr Pointer-to-pointer to reallocate (updated on success)
 * @param size New size in elements (NOT bytes)
 *
 * @return Pointer to new allocation (same as updated *ptr)
 *
 * **Behavior**:
 * - If *ptr is null, performs allocation only
 * - If size is 0, deallocates and returns zero-sized allocation
 * - Otherwise, allocates new memory, copies min(old_size, new_size) data, frees old
 *
 * **Allocator Selection**: Retains the allocator of a non-null pointer. For a
 * null pointer, uses the configured default if its platform matches; otherwise
 * uses HOST or DEVICE and checks that it matches the requested platform.
 * Throws if no matching allocator is available.
 *
 * **Exception Safety**: Strong guarantee for synchronous reallocate - *ptr unchanged on failure.
 */
template <typename T>
inline T* reallocate(camp::resources::Platform platform, T** ptr, std::size_t size)
{
  auto allocator = op::detail::get_reallocate_allocator(platform, *ptr);
  if (!*ptr) {
    *ptr = static_cast<T*>(allocator.allocate(op::detail::get_size<T>(size)));
    return *ptr;
  }
  return op::detail::dispatch<op::reallocate>(platform, ptr, size);
}

/**
 * @brief Async reallocate for typed pointer with explicit platform specification
 *
 * @tparam T Element type
 * @param platform Platform where memory resides
 * @param ptr Pointer-to-pointer to reallocate
 * @param size New size in elements
 * @param ctx Resource context for async copy operation
 *
 * @return EventProxy for completion synchronization
 *
 * **Exception Safety**: Basic guarantee - *ptr may be updated even if function throws.
 * Old memory is not freed until the copy completes. Allocator selection and
 * pointer/platform validation match the synchronous overload. For a null
 * pointer this only allocates memory; it does not submit work to ctx.
 */
template <typename T>
inline camp::resources::EventProxy<camp::resources::Resource> reallocate(camp::resources::Platform platform, T** ptr,
                                                                         std::size_t size,
                                                                         camp::resources::Resource& ctx)
{
  auto allocator = op::detail::get_reallocate_allocator(platform, *ptr);
  if (!*ptr) {
    *ptr = static_cast<T*>(allocator.allocate(op::detail::get_size<T>(size)));
    return camp::resources::EventProxy<camp::resources::Resource>{ctx};
  }
  return op::detail::dispatch<op::reallocate>(platform, ptr, size, ctx);
}

/**
 * @brief Reallocate void pointer with explicit platform specification
 *
 * @param platform Platform where memory resides
 * @param ptr Pointer-to-pointer to reallocate
 * @param size New size in bytes
 *
 * @return Pointer to new allocation
 *
 * **Difference from typed reallocate**: size parameter is in bytes, not elements.
 */
inline void* reallocate(camp::resources::Platform platform, void** ptr, std::size_t size)
{
  return reallocate<void>(platform, ptr, size);
}

/**
 * @brief Async reallocate for void pointer with explicit platform specification
 *
 * @param platform Platform where memory resides
 * @param ptr Pointer-to-pointer to reallocate
 * @param size New size in bytes
 * @param ctx Resource context for async copy operation
 *
 * @return EventProxy for completion synchronization
 */
inline camp::resources::EventProxy<camp::resources::Resource> reallocate(camp::resources::Platform platform,
                                                                         void** ptr, std::size_t size,
                                                                         camp::resources::Resource& ctx)
{
  return reallocate<void>(platform, ptr, size, ctx);
}

/**
 * @brief Prefetch memory to device with explicit platform specification
 *
 * Hints to the memory system to migrate memory pages closer to the specified device
 * for improved access performance. This is a performance hint - correctness does not
 * depend on prefetch, but performance may improve for subsequent accesses.
 *
 * @tparam T Type of data
 * @param platform Platform where memory currently resides
 * @param ptr Pointer to memory to prefetch (must be managed/unified memory)
 * @param device Target device ID for prefetch
 * @param size Number of bytes to prefetch
 *
 * **Platform Support**:
 * - CUDA: Uses cudaMemPrefetchAsync, requires managed memory capability
 * - HIP: Uses hipMemPrefetchAsync, requires managed memory capability
 * - SYCL: Requires the Resource-taking overload below
 * - OpenMP Target: No-op (not supported)
 * - Host: No-op (CPU has direct access)
 *
 * **Device IDs**: Use cudaCpuDeviceId for CPU on CUDA, device index (0, 1, 2...) for GPUs.
 *
 * **Error Handling**: Silently succeeds if device lacks managed memory support.
 */
template <typename T>
void prefetch(camp::resources::Platform platform, T* ptr, int device, std::size_t size)
{
  op::detail::dispatch<op::prefetch>(platform, ptr, device, size);
}

/**
 * @brief Async prefetch with explicit platform specification
 *
 * @tparam T Type of data
 * @param platform Platform where memory resides
 * @param ptr Pointer to prefetch
 * @param device Target device ID
 * @param size Number of bytes
 * @param ctx Resource context for async execution
 *
 * @return EventProxy for completion synchronization
 *
 * SYCL uses the allocation-bound queue for tracked pointers and ctx's queue
 * for external USM pointers in its context. This overload blocks on SYCL.
 */
template <typename T>
camp::resources::EventProxy<camp::resources::Resource> prefetch(camp::resources::Platform platform, T* ptr, int device,
                                                                std::size_t size, camp::resources::Resource& ctx)
{
  return op::detail::dispatch<op::prefetch>(platform, ptr, device, size, ctx);
}

/**
 * @brief Set accessed_by hint for unified memory
 *
 * Hints that the specified device will access this memory. On CUDA/HIP with managed
 * memory, this can establish direct mappings to avoid page faults.
 *
 * @tparam T Type of data
 * @param platform Platform where memory resides (cuda or hip only)
 * @param ptr Pointer to memory
 * @param device Device ID that will access the memory
 * @param size Number of bytes
 *
 * **Support**: CUDA and HIP only. Throws runtime_error on other platforms.
 */
template <typename T>
void set_accessed_by(camp::resources::Platform platform, T* ptr, int device, std::size_t size)
{
  op::detail::dispatch<op::set_accessed_by>(platform, ptr, device, size);
}

/**
 * @brief Set preferred_location hint for unified memory
 *
 * Hints that memory should reside on the specified device.
 *
 * @tparam T Type of data
 * @param platform Platform where memory resides (cuda or hip only)
 * @param ptr Pointer to memory
 * @param device Preferred device ID
 * @param size Number of bytes
 *
 * **Support**: CUDA and HIP only. Throws runtime_error on other platforms.
 */
template <typename T>
void set_preferred_location(camp::resources::Platform platform, T* ptr, int device, std::size_t size)
{
  op::detail::dispatch<op::set_preferred_location>(platform, ptr, device, size);
}

/**
 * @brief Set read_mostly hint for unified memory
 *
 * Hints that memory is mostly read (not written), allowing creation of read-only
 * copies on multiple devices.
 *
 * @tparam T Type of data
 * @param platform Platform where memory resides (cuda or hip only)
 * @param ptr Pointer to memory
 * @param device Device ID
 * @param size Number of bytes
 *
 * **Support**: CUDA and HIP only. Throws runtime_error on other platforms.
 */
template <typename T>
void set_read_mostly(camp::resources::Platform platform, T* ptr, int device, std::size_t size)
{
  op::detail::dispatch<op::set_read_mostly>(platform, ptr, device, size);
}

/**
 * @brief Unset accessed_by hint for unified memory
 *
 * Removes the accessed_by hint previously set for the specified device.
 *
 * @tparam T Type of data
 * @param platform Platform where memory resides (cuda or hip only)
 * @param ptr Pointer to memory
 * @param device Device ID
 * @param size Number of bytes
 *
 * **Support**: CUDA and HIP only. Throws runtime_error on other platforms.
 */
template <typename T>
void unset_accessed_by(camp::resources::Platform platform, T* ptr, int device, std::size_t size)
{
  op::detail::dispatch<op::unset_accessed_by>(platform, ptr, device, size);
}

/**
 * @brief Unset preferred_location hint for unified memory
 *
 * Removes the preferred_location hint previously set.
 *
 * @tparam T Type of data
 * @param platform Platform where memory resides (cuda or hip only)
 * @param ptr Pointer to memory
 * @param device Device ID
 * @param size Number of bytes
 *
 * **Support**: CUDA and HIP only. Throws runtime_error on other platforms.
 */
template <typename T>
void unset_preferred_location(camp::resources::Platform platform, T* ptr, int device, std::size_t size)
{
  op::detail::dispatch<op::unset_preferred_location>(platform, ptr, device, size);
}

/**
 * @brief Unset read_mostly hint for unified memory
 *
 * Removes the read_mostly hint previously set.
 *
 * @tparam T Type of data
 * @param platform Platform where memory resides (cuda or hip only)
 * @param ptr Pointer to memory
 * @param device Device ID
 * @param size Number of bytes
 *
 * **Support**: CUDA and HIP only. Throws runtime_error on other platforms.
 */
template <typename T>
void unset_read_mostly(camp::resources::Platform platform, T* ptr, int device, std::size_t size)
{
  op::detail::dispatch<op::unset_read_mostly>(platform, ptr, device, size);
}

#if (defined(UMPIRE_ENABLE_HIP) && HIP_VERSION_MAJOR >= 5)
/**
 * @brief Set coarse-grained memory access hint (HIP 5.0+)
 *
 * Configures memory to use coarse-grained access patterns, which can improve
 * performance for certain access patterns on AMD GPUs.
 *
 * @tparam T Type of data
 * @param platform Platform where memory resides (hip only)
 * @param ptr Pointer to memory
 * @param device Device ID
 * @param size Number of bytes
 *
 * **Support**: HIP 5.0+ only. Available only when UMPIRE_ENABLE_HIP is defined
 * and HIP_VERSION_MAJOR >= 5.
 */
template <typename T>
void set_coarse_grain(camp::resources::Platform platform, T* ptr, int device, std::size_t size)
{
  op::detail::dispatch<op::set_coarse_grain>(platform, ptr, device, size);
}

/**
 * @brief Unset coarse-grained memory access hint (HIP 5.0+)
 *
 * Removes the coarse-grained access hint previously set.
 *
 * @tparam T Type of data
 * @param platform Platform where memory resides (hip only)
 * @param ptr Pointer to memory
 * @param device Device ID
 * @param size Number of bytes
 *
 * **Support**: HIP 5.0+ only. Available only when UMPIRE_ENABLE_HIP is defined
 * and HIP_VERSION_MAJOR >= 5.
 */
template <typename T>
void unset_coarse_grain(camp::resources::Platform platform, T* ptr, int device, std::size_t size)
{
  op::detail::dispatch<op::unset_coarse_grain>(platform, ptr, device, size);
}
#endif

} // namespace umpire
