//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#ifndef UMPIRE_event_operation_recording_HPP
#define UMPIRE_event_operation_recording_HPP

#include <cstddef>
#include <memory>
#include <string>
#include <utility>

#include "camp/resource.hpp"
#include "umpire/event/event.hpp"
#include "umpire/replay/Replay.hpp"
#include "umpire/strategy/AllocationStrategy.hpp"

namespace umpire {
namespace event {

namespace detail {

template <typename EventType, typename AllocateFn, typename EventFn>
void* record_allocate_impl(strategy::AllocationStrategy* allocator, std::size_t bytes, AllocateFn&& allocate_fn,
                           EventFn&& event_fn)
{
  replay::ReplayAllocateToken replay_token{};
  if (replay::is_enabled()) {
    replay_token = replay::begin_allocate(allocator, bytes);
  }

  void* ptr = std::forward<AllocateFn>(allocate_fn)();

  umpire::event::record<EventType>([&](auto& event) { event_fn(event, ptr); });
  replay::commit_allocate(ptr, replay_token);

  return ptr;
}

template <typename EventType, typename DeallocateFn, typename EventFn>
void record_deallocate_impl(strategy::AllocationStrategy* allocator, void* ptr, DeallocateFn&& deallocate_fn,
                            EventFn&& event_fn)
{
  if (!ptr) {
    std::forward<DeallocateFn>(deallocate_fn)();
    return;
  }

  auto replay_token = replay::begin_deallocate(allocator, ptr);

  std::forward<DeallocateFn>(deallocate_fn)();

  umpire::event::record<EventType>([&](auto& event) { event_fn(event); });
  replay::commit_deallocate(replay_token);
}

template <typename ConstructFn, typename EventFn>
std::unique_ptr<strategy::AllocationStrategy> record_make_allocator_impl(
    const std::string& name, bool tracking, const std::string& replay_strategy, const replay::json& replay_args,
    bool suppress_nested_replay, ConstructFn&& construct_fn, EventFn&& event_fn)
{
  const bool replay_enabled = replay::is_enabled();
  replay::ReplayMakeAllocatorToken replay_token{};

  if (replay_enabled) {
    replay_token = replay::begin_make_allocator(name, tracking, replay_strategy, replay_args);
  }

  replay::ScopedNestedReplaySuppression suppress_nested(replay_enabled && suppress_nested_replay);

  auto allocator = std::forward<ConstructFn>(construct_fn)();

  if (replay_enabled) {
    replay::commit_make_allocator(allocator.get(), replay_token);
  }

  event_fn(allocator.get());

  return allocator;
}

} // end of namespace detail

template <typename AllocateFn>
void* record_allocate(strategy::AllocationStrategy* allocator, std::size_t bytes, AllocateFn&& allocate_fn)
{
  return detail::record_allocate_impl<allocate>(
      allocator, bytes, std::forward<AllocateFn>(allocate_fn),
      [&](auto& event, void* ptr) { event.size(bytes).ref((void*)allocator).ptr(ptr); });
}

template <typename AllocateFn>
void* record_named_allocate(strategy::AllocationStrategy* allocator, const std::string& name, std::size_t bytes,
                            AllocateFn&& allocate_fn)
{
  return detail::record_allocate_impl<named_allocate>(
      allocator, bytes, std::forward<AllocateFn>(allocate_fn),
      [&](auto& event, void* ptr) { event.name(name).size(bytes).ref((void*)allocator).ptr(ptr); });
}

template <typename AllocateFn>
void* record_resource_allocate(strategy::AllocationStrategy* allocator, std::size_t bytes,
                               camp::resources::Resource const& r, AllocateFn&& allocate_fn)
{
  const auto resource = camp::resources::to_string(r);

  return detail::record_allocate_impl<allocate_resource>(
      allocator, bytes, std::forward<AllocateFn>(allocate_fn),
      [&](auto& event, void* ptr) { event.size(bytes).ref((void*)allocator).ptr(ptr).res(resource); });
}

template <typename DeallocateFn>
void record_deallocate(strategy::AllocationStrategy* allocator, void* ptr, DeallocateFn&& deallocate_fn)
{
  detail::record_deallocate_impl<deallocate>(
      allocator, ptr, std::forward<DeallocateFn>(deallocate_fn),
      [&](auto& event) { event.ref((void*)allocator).ptr(ptr); });
}

template <typename DeallocateFn>
void record_resource_deallocate(strategy::AllocationStrategy* allocator, void* ptr, camp::resources::Resource const& r,
                                DeallocateFn&& deallocate_fn)
{
  const auto resource = camp::resources::to_string(r);

  detail::record_deallocate_impl<deallocate_resource>(
      allocator, ptr, std::forward<DeallocateFn>(deallocate_fn),
      [&](auto& event) { event.ref((void*)allocator).ptr(ptr).res(resource); });
}

template <typename ConstructFn, typename EventFn>
std::unique_ptr<strategy::AllocationStrategy> record_make_allocator(
    const std::string& name, bool tracking, const std::string& replay_strategy, const replay::json& replay_args,
    bool suppress_nested_replay, ConstructFn&& construct_fn, EventFn&& event_fn)
{
  return detail::record_make_allocator_impl(name, tracking, replay_strategy, replay_args, suppress_nested_replay,
                                            std::forward<ConstructFn>(construct_fn),
                                            std::forward<EventFn>(event_fn));
}

template <typename ConstructFn, typename EventFn>
std::unique_ptr<strategy::AllocationStrategy> record_make_resource(
    const std::string& name, bool tracking, const replay::json& replay_args, ConstructFn&& construct_fn,
    EventFn&& event_fn)
{
  return detail::record_make_allocator_impl(name, tracking, "MemoryResource", replay_args, false,
                                            std::forward<ConstructFn>(construct_fn),
                                            std::forward<EventFn>(event_fn));
}

} // end of namespace event
} // end of namespace umpire

#endif // UMPIRE_event_operation_recording_HPP
