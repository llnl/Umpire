//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////

#if !defined(_MSC_VER)

#include "ReplayConstructorRegistry.hpp"

#include <string>

#include "umpire/Tracking.hpp"
#include "umpire/replay/Replay.hpp"
#include "umpire/strategy/AlignedAllocator.hpp"
#include "umpire/strategy/AllocationAdvisor.hpp"
#include "umpire/strategy/AllocationPrefetcher.hpp"
#include "umpire/strategy/DynamicPoolList.hpp"
#include "umpire/strategy/FixedPool.hpp"
#include "umpire/strategy/MixedPool.hpp"
#include "umpire/strategy/MonotonicAllocationStrategy.hpp"
#include "umpire/strategy/NamedAllocationStrategy.hpp"
#include "umpire/strategy/NamingShim.hpp"
#include "umpire/strategy/QuickPool.hpp"
#include "umpire/strategy/ResourceAwarePool.hpp"
#include "umpire/strategy/SizeLimiter.hpp"
#include "umpire/strategy/SlotPool.hpp"
#include "umpire/strategy/ThreadSafeAllocator.hpp"
#include "umpire/util/error.hpp"

#if defined(UMPIRE_ENABLE_NUMA)
#include "umpire/strategy/NumaPolicy.hpp"
#endif

#if defined(UMPIRE_ENABLE_MPI) && defined(UMPIRE_ENABLE_IPC_SHARED_MEMORY) && \
    (defined(UMPIRE_ENABLE_CUDA) || defined(UMPIRE_ENABLE_HIP))
#include "umpire/strategy/DeviceIpcAllocator.hpp"
#endif

namespace {

umpire::Tracking trackingMode(const ReplayAllocatorSpec& spec)
{
  return spec.tracking ? umpire::Tracking::Tracked : umpire::Tracking::Untracked;
}

umpire::Allocator allocatorArg(const ReplayAllocatorSpec& spec, ReplayContext& context, const char* key)
{
  const auto id = spec.args.at(key).get<std::string>();
  auto found = context.allocators.find(id);
  if (found == context.allocators.end()) {
    UMPIRE_ERROR(umpire::runtime_error,
                 fmt::format("Replay allocator {} references unknown parent allocator {}", spec.name, id));
  }
  return found->second;
}

template <typename T>
T jsonValue(const nlohmann::json& args, const char* key)
{
  return args.at(key).get<T>();
}

template <typename T>
T jsonValueOr(const nlohmann::json& args, const char* key, T fallback)
{
  auto found = args.find(key);
  return found == args.end() ? fallback : found->get<T>();
}

#if defined(UMPIRE_ENABLE_MPI) && defined(UMPIRE_ENABLE_IPC_SHARED_MEMORY) && \
    (defined(UMPIRE_ENABLE_CUDA) || defined(UMPIRE_ENABLE_HIP))
umpire::MemoryResourceTraits::shared_scope sharedScopeFromString(const std::string& scope)
{
  if (scope == "node") {
    return umpire::MemoryResourceTraits::shared_scope::node;
  }
  if (scope == "socket") {
    return umpire::MemoryResourceTraits::shared_scope::socket;
  }
  return umpire::MemoryResourceTraits::shared_scope::unknown;
}
#endif

template <typename Pool>
umpire::strategy::PoolCoalesceHeuristic<Pool> parseHeuristic(const nlohmann::json& heuristic_json)
{
  const auto kind = heuristic_json.at("kind").get<std::string>();
  const auto parameter = heuristic_json.at("parameter").get<std::size_t>();

  if (kind == "percent_releasable") {
    return Pool::percent_releasable(static_cast<int>(parameter));
  }
  if (kind == "percent_releasable_hwm") {
    return Pool::percent_releasable_hwm(static_cast<int>(parameter));
  }
  if (kind == "blocks_releasable") {
    return Pool::blocks_releasable(parameter);
  }
  if (kind == "blocks_releasable_hwm") {
    return Pool::blocks_releasable_hwm(parameter);
  }

  UMPIRE_ERROR(umpire::runtime_error, fmt::format("Unsupported replay heuristic kind {}", kind));
}

} // namespace

ReplayConstructorRegistry::ReplayConstructorRegistry()
{
  registerFactory("MemoryResource", [](const ReplayAllocatorSpec& spec, ReplayContext& context) {
    const auto resource_name = jsonValue<std::string>(spec.args, "resource_name");
    const auto traits_it = spec.args.find("traits");

    umpire::Allocator allocator = (traits_it == spec.args.end())
                                      ? context.resource_manager.makeResource(resource_name)
                                      : context.resource_manager.makeResource(
                                            resource_name, umpire::replay::deserialize_memory_resource_traits(*traits_it));

    if (spec.name != resource_name) {
      context.resource_manager.addAlias(spec.name, allocator);
    }

    return allocator;
  });

  registerFactory("AllocationAdvisor", [](const ReplayAllocatorSpec& spec, ReplayContext& context) {
    auto parent = allocatorArg(spec, context, "parent_allocator");
    const auto advice = jsonValue<std::string>(spec.args, "advice_operation");
    const auto device_id = jsonValueOr<int>(spec.args, "device_id", 0);
    const auto accessing_it = spec.args.find("accessing_allocator");

    if (accessing_it == spec.args.end()) {
      return context.resource_manager.makeAllocator<umpire::strategy::AllocationAdvisor>(spec.name, trackingMode(spec),
                                                                                          parent, advice, device_id);
    }

    auto accessing = allocatorArg(spec, context, "accessing_allocator");
    return context.resource_manager.makeAllocator<umpire::strategy::AllocationAdvisor>(spec.name, trackingMode(spec),
                                                                                        parent, advice, accessing,
                                                                                        device_id);
  });

  registerFactory("AllocationPrefetcher", [](const ReplayAllocatorSpec& spec, ReplayContext& context) {
    return context.resource_manager.makeAllocator<umpire::strategy::AllocationPrefetcher>(
        spec.name, trackingMode(spec), allocatorArg(spec, context, "parent_allocator"),
        jsonValueOr<int>(spec.args, "device_id", 0));
  });

  registerFactory("AlignedAllocator", [](const ReplayAllocatorSpec& spec, ReplayContext& context) {
    return context.resource_manager.makeAllocator<umpire::strategy::AlignedAllocator>(
        spec.name, trackingMode(spec), allocatorArg(spec, context, "parent_allocator"),
        jsonValueOr<std::size_t>(spec.args, "alignment", 16));
  });

  registerFactory("DynamicPoolList", [](const ReplayAllocatorSpec& spec, ReplayContext& context) {
    return context.resource_manager.makeAllocator<umpire::strategy::DynamicPoolList>(
        spec.name, trackingMode(spec), allocatorArg(spec, context, "parent_allocator"),
        jsonValueOr<std::size_t>(spec.args, "initial_alloc_size",
                                 umpire::strategy::DynamicPoolList::s_default_first_block_size),
        jsonValueOr<std::size_t>(spec.args, "min_alloc_size",
                                 umpire::strategy::DynamicPoolList::s_default_next_block_size),
        jsonValueOr<std::size_t>(spec.args, "alignment", umpire::strategy::DynamicPoolList::s_default_alignment),
        parseHeuristic<umpire::strategy::DynamicPoolList>(spec.args.at("heuristic")));
  });

  registerFactory("FixedPool", [](const ReplayAllocatorSpec& spec, ReplayContext& context) {
    return context.resource_manager.makeAllocator<umpire::strategy::FixedPool>(
        spec.name, trackingMode(spec), allocatorArg(spec, context, "parent_allocator"),
        jsonValue<std::size_t>(spec.args, "object_bytes"),
        jsonValueOr<std::size_t>(spec.args, "objects_per_pool", 64 * sizeof(int) * 8));
  });

  registerFactory("MixedPool", [](const ReplayAllocatorSpec& spec, ReplayContext& context) {
    return context.resource_manager.makeAllocator<umpire::strategy::MixedPool>(
        spec.name, trackingMode(spec), allocatorArg(spec, context, "parent_allocator"),
        jsonValueOr<std::size_t>(spec.args, "smallest_fixed_obj_size", (1 << 8)),
        jsonValueOr<std::size_t>(spec.args, "largest_fixed_obj_size", (1 << 17)),
        jsonValueOr<std::size_t>(spec.args, "max_initial_fixed_pool_size", 1024 * 1024 * 2),
        jsonValueOr<std::size_t>(spec.args, "fixed_size_multiplier", 16),
        jsonValueOr<std::size_t>(spec.args, "quick_pool_initial_alloc_size", 512 * 1024 * 1024),
        jsonValueOr<std::size_t>(spec.args, "quick_pool_min_alloc_size", 1 * 1024 * 1024),
        jsonValueOr<std::size_t>(spec.args, "quick_pool_align_bytes", 16),
        parseHeuristic<umpire::strategy::QuickPool>(spec.args.at("heuristic")));
  });

  registerFactory("MonotonicAllocationStrategy", [](const ReplayAllocatorSpec& spec, ReplayContext& context) {
    return context.resource_manager.makeAllocator<umpire::strategy::MonotonicAllocationStrategy>(
        spec.name, trackingMode(spec), allocatorArg(spec, context, "parent_allocator"),
        jsonValue<std::size_t>(spec.args, "capacity"));
  });

  registerFactory("NamedAllocationStrategy", [](const ReplayAllocatorSpec& spec, ReplayContext& context) {
    return context.resource_manager.makeAllocator<umpire::strategy::NamedAllocationStrategy>(
        spec.name, trackingMode(spec), allocatorArg(spec, context, "parent_allocator"));
  });

  registerFactory("NamingShim", [](const ReplayAllocatorSpec& spec, ReplayContext& context) {
    return context.resource_manager.makeAllocator<umpire::strategy::NamingShim>(
        spec.name, trackingMode(spec), allocatorArg(spec, context, "parent_allocator"));
  });

  registerFactory("QuickPool", [](const ReplayAllocatorSpec& spec, ReplayContext& context) {
    return context.resource_manager.makeAllocator<umpire::strategy::QuickPool>(
        spec.name, trackingMode(spec), allocatorArg(spec, context, "parent_allocator"),
        jsonValueOr<std::size_t>(spec.args, "initial_alloc_size", umpire::strategy::QuickPool::s_default_first_block_size),
        jsonValueOr<std::size_t>(spec.args, "min_alloc_size", umpire::strategy::QuickPool::s_default_next_block_size),
        jsonValueOr<std::size_t>(spec.args, "alignment", umpire::strategy::QuickPool::s_default_alignment),
        parseHeuristic<umpire::strategy::QuickPool>(spec.args.at("heuristic")));
  });

  registerFactory("ResourceAwarePool", [](const ReplayAllocatorSpec& spec, ReplayContext& context) {
    return context.resource_manager.makeAllocator<umpire::strategy::ResourceAwarePool>(
        spec.name, trackingMode(spec), allocatorArg(spec, context, "parent_allocator"),
        jsonValueOr<std::size_t>(spec.args, "initial_alloc_size",
                                 umpire::strategy::ResourceAwarePool::s_default_first_block_size),
        jsonValueOr<std::size_t>(spec.args, "min_alloc_size",
                                 umpire::strategy::ResourceAwarePool::s_default_next_block_size),
        jsonValueOr<std::size_t>(spec.args, "alignment", umpire::strategy::ResourceAwarePool::s_default_alignment),
        parseHeuristic<umpire::strategy::ResourceAwarePool>(spec.args.at("heuristic")));
  });

  registerFactory("SizeLimiter", [](const ReplayAllocatorSpec& spec, ReplayContext& context) {
    return context.resource_manager.makeAllocator<umpire::strategy::SizeLimiter>(
        spec.name, trackingMode(spec), allocatorArg(spec, context, "parent_allocator"),
        jsonValue<std::size_t>(spec.args, "size_limit"));
  });

  registerFactory("SlotPool", [](const ReplayAllocatorSpec& spec, ReplayContext& context) {
    return context.resource_manager.makeAllocator<umpire::strategy::SlotPool>(
        spec.name, trackingMode(spec), allocatorArg(spec, context, "parent_allocator"),
        jsonValue<std::size_t>(spec.args, "slots"));
  });

  registerFactory("ThreadSafeAllocator", [](const ReplayAllocatorSpec& spec, ReplayContext& context) {
    return context.resource_manager.makeAllocator<umpire::strategy::ThreadSafeAllocator>(
        spec.name, trackingMode(spec), allocatorArg(spec, context, "parent_allocator"));
  });

#if defined(UMPIRE_ENABLE_NUMA)
  registerFactory("NumaPolicy", [](const ReplayAllocatorSpec& spec, ReplayContext& context) {
    return context.resource_manager.makeAllocator<umpire::strategy::NumaPolicy>(
        spec.name, trackingMode(spec), allocatorArg(spec, context, "parent_allocator"),
        jsonValue<int>(spec.args, "numa_node"));
  });
#endif

#if defined(UMPIRE_ENABLE_MPI) && defined(UMPIRE_ENABLE_IPC_SHARED_MEMORY) && \
    (defined(UMPIRE_ENABLE_CUDA) || defined(UMPIRE_ENABLE_HIP))
  registerFactory("DeviceIpcAllocator", [](const ReplayAllocatorSpec& spec, ReplayContext& context) {
    return context.resource_manager.makeAllocator<umpire::strategy::DeviceIpcAllocator>(
        spec.name, trackingMode(spec), allocatorArg(spec, context, "device_allocator"),
        sharedScopeFromString(jsonValue<std::string>(spec.args, "shared_scope")),
        jsonValueOr<std::size_t>(spec.args, "shared_memory_size", 1024 * 1024));
  });
#endif
}

umpire::Allocator ReplayConstructorRegistry::construct(const ReplayAllocatorSpec& spec, ReplayContext& context) const
{
  auto found = m_factories.find(spec.strategy);
  if (found == m_factories.end()) {
    UMPIRE_ERROR(umpire::runtime_error,
                 fmt::format("Replay strategy {} is not supported in this build", spec.strategy));
  }

  return found->second(spec, context);
}

void ReplayConstructorRegistry::registerFactory(const std::string& strategy, Factory factory)
{
  m_factories[strategy] = std::move(factory);
}

#endif // !defined(_MSC_VER)
