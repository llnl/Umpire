//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#ifndef UMPIRE_replay_Replay_HPP
#define UMPIRE_replay_Replay_HPP

#include <cstddef>
#include <string>
#include <type_traits>
#include <utility>

#include "umpire/config.hpp"
#include "umpire/json/json.hpp"
#include "umpire/util/MemoryResourceTraits.hpp"
#include "umpire/util/error.hpp"

namespace umpire {

class Allocator;

namespace strategy {
class AllocationStrategy;
class AllocationAdvisor;
class AllocationPrefetcher;
class AlignedAllocator;
class DynamicPoolList;
class FixedPool;
class MixedPool;
class MonotonicAllocationStrategy;
class NamedAllocationStrategy;
class NamingShim;
class QuickPool;
class ResourceAwarePool;
class SizeLimiter;
class SlotPool;
class ThreadSafeAllocator;
template <typename T>
class PoolCoalesceHeuristic;

#if defined(UMPIRE_ENABLE_NUMA)
class NumaPolicy;
#endif

#if defined(UMPIRE_ENABLE_MPI) && defined(UMPIRE_ENABLE_IPC_SHARED_MEMORY) && \
    (defined(UMPIRE_ENABLE_CUDA) || defined(UMPIRE_ENABLE_HIP))
class DeviceIpcAllocator;
#endif
} // namespace strategy

namespace replay {

using json = nlohmann::json;

enum class ReplayCommandStatus { pending, committed };

struct ReplayMakeAllocatorToken {
  bool active{false};
  std::size_t seq{0};
  std::string allocator_id{};
  std::string name{};
  std::string strategy_name{};
  bool tracking{true};
  json args{};
};

struct ReplayAllocateToken {
  bool active{false};
  std::size_t seq{0};
  std::string allocator_id{};
  std::string allocation_id{};
  std::size_t size{0};
};

struct ReplayDeallocateToken {
  bool active{false};
  std::size_t seq{0};
  std::string allocator_id{};
  std::string allocation_id{};
};

class ScopedNestedReplaySuppression {
 public:
  explicit ScopedNestedReplaySuppression(bool enabled);
  ~ScopedNestedReplaySuppression();

 private:
  bool m_enabled;
};

bool is_enabled() noexcept;

std::string resolve_allocator_id(const Allocator& allocator);
std::string resolve_allocator_id(strategy::AllocationStrategy* allocator);

ReplayMakeAllocatorToken begin_make_allocator(const std::string& name, bool tracking, const std::string& strategy_name,
                                              const json& args);
void commit_make_allocator(strategy::AllocationStrategy* allocator, const ReplayMakeAllocatorToken& token);
ReplayAllocateToken begin_allocate(strategy::AllocationStrategy* allocator, std::size_t size);
void commit_allocate(void* ptr, const ReplayAllocateToken& token);
ReplayDeallocateToken begin_deallocate(strategy::AllocationStrategy* allocator, void* ptr);
void commit_deallocate(const ReplayDeallocateToken& token);

json serialize_memory_resource_args(const std::string& resource_name, const MemoryResourceTraits& traits);
MemoryResourceTraits deserialize_memory_resource_traits(const json& traits_json);
template <typename Strategy, typename... Args>
json serialize_allocator_args(Args&&... args);
template <typename Strategy>
std::string strategy_name();

} // namespace replay
} // namespace umpire

#include "umpire/Allocator.hpp"
#include "umpire/strategy/AlignedAllocator.hpp"
#include "umpire/strategy/AllocationAdvisor.hpp"
#include "umpire/strategy/AllocationPrefetcher.hpp"
#include "umpire/strategy/DynamicPoolList.hpp"
#include "umpire/strategy/FixedPool.hpp"
#include "umpire/strategy/MixedPool.hpp"
#include "umpire/strategy/MonotonicAllocationStrategy.hpp"
#include "umpire/strategy/NamedAllocationStrategy.hpp"
#include "umpire/strategy/NamingShim.hpp"
#include "umpire/strategy/PoolCoalesceHeuristic.hpp"
#include "umpire/strategy/QuickPool.hpp"
#include "umpire/strategy/SizeLimiter.hpp"
#include "umpire/strategy/SlotPool.hpp"
#include "umpire/strategy/ThreadSafeAllocator.hpp"

#if defined(UMPIRE_ENABLE_NUMA)
#include "umpire/strategy/NumaPolicy.hpp"
#endif

#if defined(UMPIRE_ENABLE_MPI) && defined(UMPIRE_ENABLE_IPC_SHARED_MEMORY) && \
    (defined(UMPIRE_ENABLE_CUDA) || defined(UMPIRE_ENABLE_HIP))
#include "umpire/strategy/DeviceIpcAllocator.hpp"
#endif

namespace umpire {
namespace replay {

namespace detail {

template <typename T>
struct always_false : std::false_type {};

template <typename Strategy>
inline std::string replay_strategy_name(Strategy*)
{
  static_assert(always_false<Strategy>::value, "Replay strategy name is not implemented for this allocator type");
}

template <typename Pool>
json serialize_heuristic(const strategy::PoolCoalesceHeuristic<Pool>& heuristic)
{
  if (!heuristic.is_known()) {
    UMPIRE_ERROR(runtime_error,
                 "UMPIRE_REPLAY only supports pool heuristics created with the built-in helper factories");
  }

  return json{{"kind", heuristic.kind_string()}, {"parameter", heuristic.parameter()}};
}

inline json serialize_parent_only(Allocator allocator)
{
  return json{{"parent_allocator", resolve_allocator_id(allocator)}};
}

inline json serialize_make_allocator_args(strategy::AllocationAdvisor*, Allocator allocator,
                                          const std::string& advice_operation, int device_id = 0)
{
  return json{{"parent_allocator", resolve_allocator_id(allocator)},
              {"advice_operation", advice_operation},
              {"device_id", device_id}};
}

inline json serialize_make_allocator_args(strategy::AllocationAdvisor*, Allocator allocator,
                                          const std::string& advice_operation, Allocator accessing_allocator,
                                          int device_id = 0)
{
  return json{{"parent_allocator", resolve_allocator_id(allocator)},
              {"advice_operation", advice_operation},
              {"accessing_allocator", resolve_allocator_id(accessing_allocator)},
              {"device_id", device_id}};
}

inline json serialize_make_allocator_args(strategy::AllocationPrefetcher*, Allocator allocator, int device_id = 0)
{
  return json{{"parent_allocator", resolve_allocator_id(allocator)}, {"device_id", device_id}};
}

inline json serialize_make_allocator_args(strategy::AlignedAllocator*, Allocator allocator, std::size_t alignment = 16)
{
  return json{{"parent_allocator", resolve_allocator_id(allocator)}, {"alignment", alignment}};
}

inline json serialize_make_allocator_args(
    strategy::DynamicPoolList*, Allocator allocator,
    const std::size_t first_minimum_pool_allocation_size = 512 * 1024 * 1024,
    const std::size_t next_minimum_pool_allocation_size = 1 * 1024 * 1024,
    const std::size_t alignment = 16,
    strategy::PoolCoalesceHeuristic<strategy::DynamicPoolList> should_coalesce =
        strategy::DynamicPoolList::percent_releasable_hwm(100))
{
  return json{{"parent_allocator", resolve_allocator_id(allocator)},
              {"initial_alloc_size", first_minimum_pool_allocation_size},
              {"min_alloc_size", next_minimum_pool_allocation_size},
              {"alignment", alignment},
              {"heuristic", serialize_heuristic(should_coalesce)}};
}

inline json serialize_make_allocator_args(strategy::FixedPool*, Allocator allocator, const std::size_t object_bytes,
                                          const std::size_t objects_per_pool = 64 * sizeof(int) * 8)
{
  return json{{"parent_allocator", resolve_allocator_id(allocator)},
              {"object_bytes", object_bytes},
              {"objects_per_pool", objects_per_pool}};
}

inline json serialize_make_allocator_args(
    strategy::MixedPool*, Allocator allocator, std::size_t smallest_fixed_obj_size = (1 << 8),
    std::size_t largest_fixed_obj_size = (1 << 17), std::size_t max_initial_fixed_pool_size = 1024 * 1024 * 2,
    std::size_t fixed_size_multiplier = 16, const std::size_t quick_pool_initial_alloc_size = (512 * 1024 * 1024),
    const std::size_t quick_pool_min_alloc_size = (1 * 1024 * 1024), const std::size_t quick_pool_align_bytes = 16,
    strategy::PoolCoalesceHeuristic<strategy::QuickPool> should_coalesce =
        strategy::QuickPool::percent_releasable(100))
{
  return json{{"parent_allocator", resolve_allocator_id(allocator)},
              {"smallest_fixed_obj_size", smallest_fixed_obj_size},
              {"largest_fixed_obj_size", largest_fixed_obj_size},
              {"max_initial_fixed_pool_size", max_initial_fixed_pool_size},
              {"fixed_size_multiplier", fixed_size_multiplier},
              {"quick_pool_initial_alloc_size", quick_pool_initial_alloc_size},
              {"quick_pool_min_alloc_size", quick_pool_min_alloc_size},
              {"quick_pool_align_bytes", quick_pool_align_bytes},
              {"heuristic", serialize_heuristic(should_coalesce)}};
}

inline json serialize_make_allocator_args(strategy::MonotonicAllocationStrategy*, Allocator allocator,
                                          std::size_t capacity)
{
  return json{{"parent_allocator", resolve_allocator_id(allocator)}, {"capacity", capacity}};
}

inline json serialize_make_allocator_args(strategy::NamedAllocationStrategy*, Allocator allocator)
{
  return serialize_parent_only(allocator);
}

inline json serialize_make_allocator_args(strategy::NamingShim*, Allocator allocator)
{
  return serialize_parent_only(allocator);
}

inline json serialize_make_allocator_args(
    strategy::QuickPool*, Allocator allocator,
    const std::size_t first_minimum_pool_allocation_size = 512 * 1024 * 1024,
    const std::size_t next_minimum_pool_allocation_size = 1 * 1024 * 1024,
    const std::size_t alignment = 16,
    strategy::PoolCoalesceHeuristic<strategy::QuickPool> should_coalesce =
        strategy::QuickPool::percent_releasable_hwm(100))
{
  return json{{"parent_allocator", resolve_allocator_id(allocator)},
              {"initial_alloc_size", first_minimum_pool_allocation_size},
              {"min_alloc_size", next_minimum_pool_allocation_size},
              {"alignment", alignment},
              {"heuristic", serialize_heuristic(should_coalesce)}};
}

inline json serialize_resource_aware_pool_args(Allocator allocator, const std::size_t first_minimum_pool_allocation_size,
                                               const std::size_t next_minimum_pool_allocation_size,
                                               const std::size_t alignment, const json& heuristic)
{
  return json{{"parent_allocator", resolve_allocator_id(allocator)},
              {"initial_alloc_size", first_minimum_pool_allocation_size},
              {"min_alloc_size", next_minimum_pool_allocation_size},
              {"alignment", alignment},
              {"heuristic", heuristic}};
}

inline json serialize_make_allocator_args(strategy::ResourceAwarePool*, Allocator allocator)
{
  return serialize_resource_aware_pool_args(allocator, 512 * 1024 * 1024, 1 * 1024 * 1024, 16,
                                            json{{"kind", "percent_releasable_hwm"}, {"parameter", 100}});
}

inline json serialize_make_allocator_args(strategy::ResourceAwarePool*, Allocator allocator,
                                          const std::size_t first_minimum_pool_allocation_size)
{
  return serialize_resource_aware_pool_args(allocator, first_minimum_pool_allocation_size, 1 * 1024 * 1024, 16,
                                            json{{"kind", "percent_releasable_hwm"}, {"parameter", 100}});
}

inline json serialize_make_allocator_args(strategy::ResourceAwarePool*, Allocator allocator,
                                          const std::size_t first_minimum_pool_allocation_size,
                                          const std::size_t next_minimum_pool_allocation_size)
{
  return serialize_resource_aware_pool_args(allocator, first_minimum_pool_allocation_size,
                                            next_minimum_pool_allocation_size, 16,
                                            json{{"kind", "percent_releasable_hwm"}, {"parameter", 100}});
}

inline json serialize_make_allocator_args(strategy::ResourceAwarePool*, Allocator allocator,
                                          const std::size_t first_minimum_pool_allocation_size,
                                          const std::size_t next_minimum_pool_allocation_size,
                                          const std::size_t alignment)
{
  return serialize_resource_aware_pool_args(allocator, first_minimum_pool_allocation_size,
                                            next_minimum_pool_allocation_size, alignment,
                                            json{{"kind", "percent_releasable_hwm"}, {"parameter", 100}});
}

inline json serialize_make_allocator_args(strategy::ResourceAwarePool*, Allocator allocator,
                                          const std::size_t first_minimum_pool_allocation_size,
                                          const std::size_t next_minimum_pool_allocation_size,
                                          const std::size_t alignment,
                                          strategy::PoolCoalesceHeuristic<strategy::ResourceAwarePool> should_coalesce)
{
  return serialize_resource_aware_pool_args(allocator, first_minimum_pool_allocation_size,
                                            next_minimum_pool_allocation_size, alignment,
                                            serialize_heuristic(should_coalesce));
}

inline json serialize_make_allocator_args(strategy::SizeLimiter*, Allocator allocator, std::size_t size_limit)
{
  return json{{"parent_allocator", resolve_allocator_id(allocator)}, {"size_limit", size_limit}};
}

inline json serialize_make_allocator_args(strategy::SlotPool*, Allocator allocator, std::size_t slots)
{
  return json{{"parent_allocator", resolve_allocator_id(allocator)}, {"slots", slots}};
}

inline json serialize_make_allocator_args(strategy::ThreadSafeAllocator*, Allocator allocator)
{
  return serialize_parent_only(allocator);
}

inline std::string replay_strategy_name(strategy::AllocationAdvisor*)
{
  return "AllocationAdvisor";
}

inline std::string replay_strategy_name(strategy::AllocationPrefetcher*)
{
  return "AllocationPrefetcher";
}

inline std::string replay_strategy_name(strategy::AlignedAllocator*)
{
  return "AlignedAllocator";
}

inline std::string replay_strategy_name(strategy::DynamicPoolList*)
{
  return "DynamicPoolList";
}

inline std::string replay_strategy_name(strategy::FixedPool*)
{
  return "FixedPool";
}

inline std::string replay_strategy_name(strategy::MixedPool*)
{
  return "MixedPool";
}

inline std::string replay_strategy_name(strategy::MonotonicAllocationStrategy*)
{
  return "MonotonicAllocationStrategy";
}

inline std::string replay_strategy_name(strategy::NamedAllocationStrategy*)
{
  return "NamedAllocationStrategy";
}

inline std::string replay_strategy_name(strategy::NamingShim*)
{
  return "NamingShim";
}

inline std::string replay_strategy_name(strategy::QuickPool*)
{
  return "QuickPool";
}

inline std::string replay_strategy_name(strategy::ResourceAwarePool*)
{
  return "ResourceAwarePool";
}

inline std::string replay_strategy_name(strategy::SizeLimiter*)
{
  return "SizeLimiter";
}

inline std::string replay_strategy_name(strategy::SlotPool*)
{
  return "SlotPool";
}

inline std::string replay_strategy_name(strategy::ThreadSafeAllocator*)
{
  return "ThreadSafeAllocator";
}

#if defined(UMPIRE_ENABLE_NUMA)
inline json serialize_make_allocator_args(strategy::NumaPolicy*, Allocator allocator, int numa_node)
{
  return json{{"parent_allocator", resolve_allocator_id(allocator)}, {"numa_node", numa_node}};
}

inline std::string replay_strategy_name(strategy::NumaPolicy*)
{
  return "NumaPolicy";
}
#endif

#if defined(UMPIRE_ENABLE_MPI) && defined(UMPIRE_ENABLE_IPC_SHARED_MEMORY) && \
    (defined(UMPIRE_ENABLE_CUDA) || defined(UMPIRE_ENABLE_HIP))
inline json serialize_make_allocator_args(strategy::DeviceIpcAllocator*)
{
  return json{{"shared_scope", "socket"}, {"shared_memory_size", 1024 * 1024}};
}

inline json serialize_make_allocator_args(strategy::DeviceIpcAllocator*, Allocator device_allocator,
                                          MemoryResourceTraits::shared_scope scope,
                                          std::size_t shared_memory_size = 1024 * 1024)
{
  return json{{"device_allocator", resolve_allocator_id(device_allocator)},
              {"shared_scope", to_string(scope)},
              {"shared_memory_size", shared_memory_size}};
}

inline std::string replay_strategy_name(strategy::DeviceIpcAllocator*)
{
  return "DeviceIpcAllocator";
}
#endif

} // namespace detail

template <typename Strategy, typename... Args>
json serialize_allocator_args(Args&&... args)
{
  return detail::serialize_make_allocator_args(static_cast<Strategy*>(nullptr), std::forward<Args>(args)...);
}

template <typename Strategy>
std::string strategy_name()
{
  return detail::replay_strategy_name(static_cast<Strategy*>(nullptr));
}

} // namespace replay
} // namespace umpire

#endif // UMPIRE_replay_Replay_HPP
