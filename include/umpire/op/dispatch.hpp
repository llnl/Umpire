#pragma once

#include <limits>
#include <type_traits>

#include "umpire/ResourceManager.hpp"
#include "umpire/config.hpp"
#include "umpire/op/detail/traits.hpp"
#include "umpire/op/detail/utils.hpp"
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
  // Get the last argument from a parameter pack
  template <typename... Args>
  static auto get_last_arg(Args... args)
  {
    return std::get<sizeof...(Args) - 1>(std::forward_as_tuple(args...));
  }

  // Get the Nth argument from a parameter pack
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
    // Calculate source and destination details
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

    // Check if source has enough data and destination has enough space
    if (requested_bytes > src_available_bytes) {
      UMPIRE_ERROR(runtime_error,
                   fmt::format("Not enough data in source to copy {} bytes from {} bytes", requested_bytes,
                               src_available_bytes));
    }

    // Check if destination has enough space
    if (requested_bytes > dst_available_bytes) {
      UMPIRE_ERROR(runtime_error,
                   fmt::format("Not enough space in destination to copy {} bytes into {} bytes", requested_bytes,
                               dst_available_bytes));
    }
  }
#endif // UMPIRE_ENABLE_BOUNDS_CHECKS

  // Single-pointer operations (synchronous)
  template <typename T, typename... Args>
  inline static auto exec(T* src, Args... args)
  {
    auto& rm = ResourceManager::getInstance();
    auto& allocation_map = rm.m_allocations;
    auto src_record = allocation_map.find(detail::get_base_ptr(src));
    auto p = src_record->strategy->getPlatform();

    // Operation-specific handling
    if constexpr (std::is_same_v<Op<resource::host_platform>, memset<resource::host_platform>>) {
      // For memset, we expect args to be {value, size}
#ifdef UMPIRE_ENABLE_BOUNDS_CHECKS
      std::size_t length = get_arg<1>(args...);
      check_memset_bounds(src, src_record, length);
#endif
    }

    // Dispatch based on platform
    return detail::dispatch<Op>(p, src, args...);
  }

  // Single-pointer operations (asynchronous)
  template <typename T, typename... Args>
  inline static auto exec(T* src, Args... args, camp::resources::Resource& ctx)
  {
    auto& rm = ResourceManager::getInstance();
    auto& allocation_map = rm.m_allocations;
    auto src_record = allocation_map.find(detail::get_base_ptr(src));
    auto p = src_record->strategy->getPlatform();

    // Operation-specific handling
    if constexpr (std::is_same_v<Op<resource::host_platform>, memset<resource::host_platform>>) {
      // For memset, we expect args to be {value, size}
#ifdef UMPIRE_ENABLE_BOUNDS_CHECKS
      std::size_t length = get_arg<1>(args...);
      check_memset_bounds(src, src_record, length);
#endif
    }

    return detail::dispatch<Op>(p, src, std::forward<Args>(args)..., ctx);
  }

  // Dual-pointer operations (synchronous)
  template <typename T, typename... Args>
  inline static auto exec(T* src, T* dst, Args... args)
  {
    auto& rm = ResourceManager::getInstance();
    auto& allocation_map = rm.m_allocations;
    auto src_record = allocation_map.find(detail::get_base_ptr(src));
    auto dst_record = allocation_map.find(dst);

    auto p1 = src_record->strategy->getPlatform();
    auto p2 = dst_record->strategy->getPlatform();

    // Operation-specific handling for copy
    if constexpr (std::is_same_v<Op<resource::host_platform, resource::host_platform>,
                                 copy<resource::host_platform, resource::host_platform>>) {
      // For copy, we expect args to be {size}
#ifdef UMPIRE_ENABLE_BOUNDS_CHECKS
      std::size_t size = get_arg<0>(args...);
      check_copy_bounds(src, dst, src_record, dst_record, size);
#endif
    }

    // Dispatch based on source and destination platforms
    return detail::dispatch<Op>(p1, p2, src, dst, args...);
  }

  // Dual-pointer operations (asynchronous)
  template <typename T, typename... Args>
  inline static auto exec(T* src, T* dst, Args... args, camp::resources::Resource& ctx)
  {
    auto& rm = ResourceManager::getInstance();
    auto& allocation_map = rm.m_allocations;
    auto src_record = allocation_map.find(detail::get_base_ptr(src));
    auto dst_record = allocation_map.find(dst);

    auto p1 = src_record->strategy->getPlatform();
    auto p2 = dst_record->strategy->getPlatform();

    // Operation-specific handling for copy
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
  op::op_caller<op::copy>::exec(src, dst, len);
}

template <typename T>
auto copy(T* src, T* dst, std::size_t len, camp::resources::Resource& ctx)
{
  return op::op_caller<op::copy>::exec(src, dst, len, ctx);
}

template <typename T, typename V>
void memset(T* src, V v, std::size_t len)
{
  op::op_caller<op::memset>::exec(src, v, len);
}

template <typename T>
camp::resources::EventProxy<camp::resources::Resource> memset(T* src, int v, std::size_t len,
                                                              camp::resources::Resource& ctx)
{
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
  // Handle nullptr specially - op_caller can't look up null in allocation map
  if (*src == nullptr) {
    return op::reallocate<resource::host_platform>::exec(src, size);
  }
  return op::op_caller<op::reallocate>::exec(src, size);
}

// Async reallocate implementation
template <typename T>
inline camp::resources::EventProxy<camp::resources::Resource> reallocate(T** src, std::size_t size,
                                                                         camp::resources::Resource& ctx)
{
  // Handle nullptr specially - op_caller can't look up null in allocation map
  if (*src == nullptr) {
    return op::reallocate<resource::host_platform>::exec(src, size, ctx);
  }
  return op::op_caller<op::reallocate>::exec(src, size, ctx);
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

  // 1. Null pointer case - just allocate
  if (!current_ptr) {
    auto& rm = ResourceManager::getInstance();
    Allocator allocator = rm.getDefaultAllocator();

    std::size_t alloc_bytes = detail::get_size<PtrType>(new_size);

    PtrType* new_ptr = static_cast<PtrType*>(allocator.allocate(alloc_bytes));
    *ptr_ptr = new_ptr;
    return new_ptr;
  }

  // 2. Get allocator and validate
  auto& rm = ResourceManager::getInstance();
  auto& allocation_map = rm.m_allocations;
  Allocator allocator = rm.getAllocator(current_ptr);

  // 3. Check for offset pointer
  auto alloc_record = allocation_map.find(current_ptr);
  if (current_ptr != alloc_record->ptr) {
    UMPIRE_ERROR(runtime_error, fmt::format("Cannot reallocate an offset ptr (ptr={}, base={})",
                                            reinterpret_cast<void*>(current_ptr), alloc_record->ptr));
  }

  // 4. Calculate sizes
  std::size_t old_size = rm.getSize(current_ptr);
  std::size_t new_bytes = detail::get_size<PtrType>(new_size);

  // 5. Zero-byte special case
  if (new_bytes == 0) {
    allocator.deallocate(current_ptr);
    PtrType* new_ptr = static_cast<PtrType*>(allocator.allocate(0));
    *ptr_ptr = new_ptr;
    return new_ptr;
  }

  // 6. Allocate new memory
  PtrType* new_ptr = static_cast<PtrType*>(allocator.allocate(new_bytes));

  // 7. Copy data with exception safety
  std::size_t copy_bytes = (old_size > new_bytes) ? new_bytes : old_size;

  // Guard new pointer for exception safety
  auto new_cleanup = detail::make_scope_exit([&]() {
    allocator.deallocate(new_ptr);
  });

  // Copy data using void* to pass bytes directly
  umpire::copy(static_cast<void*>(current_ptr), static_cast<void*>(new_ptr), copy_bytes);

  // Copy succeeded, deallocate old memory
  allocator.deallocate(current_ptr);

  // All cleanup succeeded, dismiss guard and update pointer
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

  // 1. Null pointer case - just allocate
  if (!current_ptr) {
    auto& rm = ResourceManager::getInstance();
    Allocator allocator = rm.getDefaultAllocator();

    std::size_t alloc_bytes = detail::get_size<PtrType>(new_size);

    PtrType* new_ptr = static_cast<PtrType*>(allocator.allocate(alloc_bytes));
    *ptr_ptr = new_ptr;
    return camp::resources::EventProxy<camp::resources::Resource>{ctx};
  }

  // 2. Get allocator and validate
  auto& rm = ResourceManager::getInstance();
  auto& allocation_map = rm.m_allocations;
  Allocator allocator = rm.getAllocator(current_ptr);

  // 3. Check for offset pointer
  auto alloc_record = allocation_map.find(current_ptr);
  if (current_ptr != alloc_record->ptr) {
    UMPIRE_ERROR(runtime_error, fmt::format("Cannot reallocate an offset ptr (ptr={}, base={})",
                                            reinterpret_cast<void*>(current_ptr), alloc_record->ptr));
  }

  // 4. Calculate sizes
  std::size_t old_size = rm.getSize(current_ptr);
  std::size_t new_bytes = detail::get_size<PtrType>(new_size);

  // 5. Zero-byte special case
  if (new_bytes == 0) {
    allocator.deallocate(current_ptr);
    PtrType* new_ptr = static_cast<PtrType*>(allocator.allocate(0));
    *ptr_ptr = new_ptr;
    return camp::resources::EventProxy<camp::resources::Resource>{ctx};
  }

  // 6. Allocate new memory
  PtrType* new_ptr = static_cast<PtrType*>(allocator.allocate(new_bytes));

  // Guard new pointer for exception safety: if the copy below throws (or the
  // wait throws), release the new allocation so it is not leaked and *ptr_ptr
  // is not left dangling.
  auto new_cleanup = detail::make_scope_exit([&]() {
    allocator.deallocate(new_ptr);
  });

  // 7. Copy data asynchronously
  std::size_t copy_bytes = (old_size > new_bytes) ? new_bytes : old_size;

  auto event = umpire::copy(static_cast<void*>(current_ptr), static_cast<void*>(new_ptr), copy_bytes, ctx);

  // Guard old pointer for cleanup after copy completes
  auto cleanup = detail::make_scope_exit([&]() {
    allocator.deallocate(current_ptr);
  });

  // NOTE: This wait is an intentional blocking point. It guarantees the copy
  // has completed before the old buffer is freed (and before we dismiss the
  // new-pointer guard below). A true non-blocking async reallocate that defers
  // the old-buffer free until the event completes is left as a follow-up.
  static_cast<camp::resources::Event>(event).wait();
  // cleanup happens automatically via RAII

  // Copy (and wait) succeeded: the new allocation is valid, dismiss its guard.
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

template <typename Platform, typename T>
void device_memset(T* ptr, T value, std::size_t len)
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

#if (defined(UMPIRE_ENABLE_HIP) && HIP_VERSION_MAJOR >= 5) || defined(UMPIRE_ENABLE_CUDA)
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
// These functions accept camp::resources::Platform enum values as runtime
// parameters, enabling explicit platform selection without allocation map lookups.
// They delegate to the existing op::detail::dispatch() infrastructure.
//
// **When to Use This API**:
//
// Use platform-by-value API when:
// - Working with external memory NOT allocated by Umpire
// - Performance profiling shows allocation map lookups are a bottleneck
// - You need explicit runtime control over platform dispatch
// - Testing or debugging requires precise platform specification
//
// Use allocation-map-based API when:
// - Working with Umpire-managed allocations (automatic platform detection)
// - You want Umpire to track operations for replay/debugging
// - Code readability is more important than avoiding map lookups
// - You're unsure which to use (safer default)
//
// **Performance Considerations**:
// - Platform-by-value API: O(1) dispatch, no map lookup
// - Allocation-map API: O(log n) map lookup where n = number of allocations
// - For small n (<1000 allocations), performance difference is negligible
// - For large n or very hot paths, platform-by-value may provide 5-10% improvement
//
// **Platform Values**:
// - Platform::host - CPU memory
// - Platform::cuda - NVIDIA GPU memory
// - Platform::hip - AMD GPU memory
// - Platform::sycl - SYCL device memory
// - Platform::omp_target - OpenMP target device memory
//
// **Thread Safety**: Functions are thread-safe when called on different pointers.
// Concurrent operations on the same pointer are NOT thread-safe and require
// external synchronization.
//
// **Error Handling**: Functions throw umpire::runtime_error for:
// - Unsupported platform combinations (e.g., direct CUDA<->HIP copy)
// - Platform-specific operation failures
// - Resource type mismatches (e.g., CUDA resource with HIP platform)
//
// **Example Usage**:
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
 * This function performs a memory copy without querying the allocation map,
 * allowing operations on external memory not tracked by Umpire's ResourceManager.
 * Useful for performance-critical code paths or when working with non-Umpire allocations.
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
 * **Performance**: Avoids allocation map lookup overhead (~O(log n) complexity).
 * For Umpire-managed memory, prefer the allocation-map-based API for automatic
 * platform detection unless performance profiling shows map lookup is a bottleneck.
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
 * capabilities (true async on CUDA/HIP/SYCL, synchronous on OpenMP Target).
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
 * **Resource Type**: The ctx parameter must match the src_platform/dst_platform.
 * Passing a CUDA resource when src_platform=Platform::host will throw an error.
 *
 * **Platform-Specific Behavior**:
 * - CUDA/HIP/SYCL: True asynchronous execution on device stream/queue
 * - OpenMP Target: Synchronous execution (async API for compatibility only)
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
 * Sets each byte in the memory region to the specified value. Works without
 * allocation map lookups, enabling operations on external memory.
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
 * **Platform-Specific Behavior**: See copy() async documentation for details
 * on platform-specific async behavior.
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
 * **Important**: SYCL requires a Resource parameter even for "synchronous" calls
 * due to implementation details. Use the allocation-map API for SYCL device_memset.
 */
template <typename T, typename V>
void device_memset(camp::resources::Platform platform, T* ptr, V value, std::size_t len)
{
  op::detail::dispatch<op::device_memset>(platform, ptr, value, len);
}

/**
 * @brief Reallocate typed pointer with explicit platform specification
 *
 * Resizes an allocation by allocating new memory, copying data, and freeing old memory.
 * Operates without allocation map, so the allocator must be determined from the platform.
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
 * **Allocator Selection**: Uses the default allocator for the specified platform.
 * For custom allocator control, use the allocation-map-based API or ResourceManager::reallocate().
 *
 * **Exception Safety**: Strong guarantee for synchronous reallocate - *ptr unchanged on failure.
 */
template <typename T>
inline T* reallocate(camp::resources::Platform platform, T** ptr, std::size_t size)
{
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
 * Old memory is not freed until async copy completes.
 */
template <typename T>
inline camp::resources::EventProxy<camp::resources::Resource> reallocate(camp::resources::Platform platform, T** ptr,
                                                                         std::size_t size,
                                                                         camp::resources::Resource& ctx)
{
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
  return op::detail::dispatch<op::reallocate>(platform, ptr, size);
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
  return op::detail::dispatch<op::reallocate>(platform, ptr, size, ctx);
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
 * - SYCL: Uses queue.prefetch()
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
