// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#include "umpire/strategy/ResourceAwarePool.hpp"

#include "umpire/Allocator.hpp"
#include "umpire/event/event.hpp"
#include "umpire/strategy/PoolCoalesceHeuristic.hpp"
#include "umpire/strategy/mixins/AlignedAllocation.hpp"
#include "umpire/util/Macros.hpp"
#include "umpire/util/memory_sanitizers.hpp"

namespace umpire {
namespace strategy {

ResourceAwarePool::ResourceAwarePool(const std::string& name, int id, Allocator allocator,
                                     const std::size_t first_minimum_pool_allocation_size,
                                     const std::size_t next_minimum_pool_allocation_size, std::size_t alignment,
                                     PoolCoalesceHeuristic<ResourceAwarePool> should_coalesce) noexcept
    : AllocationStrategy{name, id, allocator.getAllocationStrategy(), "ResourceAwarePool"},
      mixins::AlignedAllocation{alignment, allocator.getAllocationStrategy()},
      m_should_coalesce{should_coalesce},
      m_first_minimum_pool_allocation_size{first_minimum_pool_allocation_size},
      m_next_minimum_pool_allocation_size{next_minimum_pool_allocation_size}
{
  UMPIRE_LOG(Debug, " ( "
                        << "name=\"" << name << "\""
                        << ", id=" << id << ", allocator=\"" << allocator.getName() << "\""
                        << ", first_minimum_pool_allocation_size=" << m_first_minimum_pool_allocation_size
                        << ", next_minimum_pool_allocation_size=" << m_next_minimum_pool_allocation_size
                        << ", alignment=" << alignment << " )");
}

ResourceAwarePool::~ResourceAwarePool()
{
  UMPIRE_LOG(Debug, "Releasing free blocks to device");
  m_is_destructing = true;
  release();
}

void* ResourceAwarePool::allocate(std::size_t bytes)
{
  UMPIRE_LOG(
      Warning,
      fmt::format("The ResourceAwarePool requires a Camp resource. See "
                  "https://umpire.readthedocs.io/en/develop/sphinx/cookbook/resource_aware_pool.html for more info."
                  "Calling allocate with the default Host resource..."));

  return allocate_resource(bytes, camp::resources::Host().get_default());
}

void* ResourceAwarePool::allocate_resource(std::size_t bytes, camp::resources::Resource r)
{
  UMPIRE_LOG(Debug, "(bytes=" << bytes << ")");
  UMPIRE_LOG(Debug, "(Resource=" << camp::resources::to_string(r) << ")");
  const std::size_t rounded_bytes{aligned_round_up(bytes)};
  Chunk* chunk{nullptr};

  auto range = m_pending_map.equal_range(std::optional<Resource>(r));
  bool was_pending = false;
  for (auto it = range.first; it != range.second; ++it) {
    auto pending_chunk = it->second;
    if (pending_chunk->size >= rounded_bytes) {
        // reuse chunk with same resource
        chunk = pending_chunk;
        chunk->free = false;

        // delete from pending map and invalidate the iterator
        m_pending_map.erase(it);
        chunk->pending_map_it = m_pending_map.end();
        was_pending = true; // If we split the chunk later, we need this info
        break;
    }
  } 

  const auto& best = m_free_map.lower_bound(rounded_bytes);

  if (chunk == nullptr) {
    if (best == m_free_map.end()) {
      std::size_t bytes_to_use{(m_actual_bytes == 0) ? m_first_minimum_pool_allocation_size
                                                     : m_next_minimum_pool_allocation_size};

      std::size_t size{(rounded_bytes > bytes_to_use) ? rounded_bytes : bytes_to_use};

      UMPIRE_LOG(Debug, "Allocating new chunk of size " << size);

      void* ret{nullptr};
      try {
#if defined(UMPIRE_ENABLE_BACKTRACE)
        {
          umpire::util::backtrace bt;
          umpire::util::backtracer<>::get_backtrace(bt);
          UMPIRE_LOG(Info, "actual_size:" << (m_actual_bytes + rounded_bytes) << " (prev: " << m_actual_bytes << ") "
                                          << umpire::util::backtracer<>::print(bt));
        }
#endif
        ret = aligned_allocate(size); // Will Poison
      } catch (...) {
        UMPIRE_LOG(Error,
                   "Caught error allocating new chunk, giving up free chunks and "
                   "retrying...");
        release();
        try {
          ret = aligned_allocate(size); // Will Poison
          UMPIRE_LOG(Debug, "memory reclaimed, chunk successfully allocated.");
        } catch (...) {
          UMPIRE_LOG(Error, "recovery failed.");
          throw;
        }
      }

      m_actual_bytes += size;
      m_releasable_bytes += size;
      m_releasable_blocks++;
      m_total_blocks++;
      m_actual_highwatermark = (m_actual_bytes > m_actual_highwatermark) ? m_actual_bytes : m_actual_highwatermark;

      void* chunk_storage{m_chunk_pool.allocate()};
      chunk = new (chunk_storage) Chunk{ret, size, size, r};
    } else {
      chunk = (*best).second;
      chunk->resource = r;
      m_free_map.erase(best);
    }
  }

  UMPIRE_LOG(Debug, "Using chunk " << chunk << " with data " << chunk->data << " and size " << chunk->size
                                   << " for allocation of size " << rounded_bytes);

  if ((chunk->size == chunk->chunk_size) && chunk->free) {
    m_releasable_bytes -= chunk->chunk_size;
    m_releasable_blocks--;
  }

  void* ret = chunk->data;
  m_used_map.insert(std::make_pair(ret, chunk));

  chunk->free = false;

  if ((rounded_bytes != chunk->size) && !was_pending) { // Don't split a reused pending chunk
    std::size_t remaining{chunk->size - rounded_bytes};
    UMPIRE_LOG(Debug, "Splitting chunk " << chunk->size << "into " << rounded_bytes << " and " << remaining);

    void* chunk_storage{m_chunk_pool.allocate()};
    Chunk* split_chunk{new (chunk_storage)
                           Chunk{static_cast<char*>(ret) + rounded_bytes, remaining, chunk->chunk_size, r}};
    auto old_next = chunk->next;
    chunk->next = split_chunk;
    split_chunk->prev = chunk;
    split_chunk->next = old_next;

    if (split_chunk->next)
      split_chunk->next->prev = split_chunk;

    chunk->size = rounded_bytes;
    split_chunk->size_map_it = m_free_map.insert(std::make_pair(remaining, split_chunk));
    split_chunk->free = true;
  }

  m_aligned_bytes += rounded_bytes;
  if (m_aligned_bytes > m_aligned_highwatermark) {
    m_aligned_highwatermark = m_aligned_bytes;
  }

  UMPIRE_UNPOISON_MEMORY_REGION(m_allocator, ret, bytes);
  return ret;
}

void ResourceAwarePool::deallocate(void* ptr, std::size_t size)
{
  auto r = getResource(ptr);

  UMPIRE_LOG(Warning, fmt::format("The ResourceAwarePool requires a Camp resource. Calling deallocate with: {}.",
                                  camp::resources::to_string(r)));

  deallocate_resource(ptr, r, size);
}

void ResourceAwarePool::do_deallocate(Chunk* chunk, void* ptr) noexcept
{
  UMPIRE_POISON_MEMORY_REGION(m_allocator, ptr, chunk->size);
  UMPIRE_USE_VAR(ptr);
  chunk->free = true;

  // Remove chunk from pending and invalidate iterator
  if (chunk->pending_map_it != m_pending_map.end()) {
    m_pending_map.erase(chunk->pending_map_it);
    chunk->pending_map_it = m_pending_map.end();
  }

  UMPIRE_LOG(Debug, "In the do_deallocate function. Deallocating data held by " << chunk);

  if (chunk->prev && chunk->prev->free == true) {
    auto prev = chunk->prev;

    UMPIRE_LOG(Debug, "Removing chunk" << prev << " from free map");
    m_free_map.erase(prev->size_map_it);

    prev->size += chunk->size;
    prev->next = chunk->next;

    prev->event = chunk->event;
    prev->resource = chunk->resource;

    if (prev->next)
      prev->next->prev = prev;

    UMPIRE_LOG(Debug, "Merging with prev" << prev << " and " << chunk);
    UMPIRE_LOG(Debug, "New size: " << prev->size);

    chunk->~Chunk(); // manually call destructor
    m_chunk_pool.deallocate(chunk);
    chunk = prev;
  }

  if (chunk->next && chunk->next->free == true) {
    auto next = chunk->next;

    UMPIRE_LOG(Debug, "Removing chunk" << next << " from free map");
    m_free_map.erase(next->size_map_it);

    chunk->size += next->size;
    chunk->next = next->next;

    chunk->event = next->event;
    chunk->resource = next->resource;

    if (chunk->next)
      chunk->next->prev = chunk;

    UMPIRE_LOG(Debug, "Merging with next" << chunk << " and " << next);
    UMPIRE_LOG(Debug, "New size: " << chunk->size);

    next->~Chunk(); // manually call destructor
    m_chunk_pool.deallocate(next);
  }

  UMPIRE_LOG(Debug, "Inserting chunk " << chunk << " with size " << chunk->size);

  if (chunk->size == chunk->chunk_size) {
    m_releasable_blocks++;
    m_releasable_bytes += chunk->chunk_size;
  }

  chunk->size_map_it = m_free_map.insert(std::make_pair(chunk->size, chunk));
}

void ResourceAwarePool::deallocate_resource(void* ptr, camp::resources::Resource r, std::size_t UMPIRE_UNUSED_ARG(size))
{
  UMPIRE_LOG(Debug, "(ptr=" << ptr << ")");
  UMPIRE_LOG(Debug, "(Resource=" << camp::resources::to_string(r) << ")");
  auto chunk = (*m_used_map.find(ptr)).second;

  if (chunk == nullptr) {
    UMPIRE_ERROR(runtime_error, fmt::format("The chunk can't be found! Called deallocate with ptr: {}", ptr));
  }

  if (chunk->resource != r) {
    UMPIRE_ERROR(
        runtime_error,
        fmt::format(
            "Called deallocate with a different resource than what was expected. Called with: {} but expected: {}",
            camp::resources::to_string(r), camp::resources::to_string(chunk->resource)));
  }

  if (m_is_coalescing == false) {
    chunk->event = r.get_event();
  }

  m_used_map.erase(ptr);
  m_aligned_bytes -= chunk->size;

  // Call deallocate logic only for a non-pending chunk
  if (chunk->event.check()) {
    do_deallocate(chunk, ptr);
  } else {
    // Chunk is now pending, add to pending map and set iterator
    auto it = m_pending_map.insert({std::optional<Resource>(chunk->resource), chunk});
    chunk->pending_map_it = it;
  }

  std::size_t suggested_size{m_should_coalesce(*this)};
  if (0 != suggested_size) {
    UMPIRE_LOG(Debug, "coalesce heuristic true, performing coalesce.");
    do_coalesce(suggested_size);
  }
}

void ResourceAwarePool::release()
{
  UMPIRE_LOG(Debug, "() " << m_free_map.size() << " chunks in free map, m_is_destructing set to " << m_is_destructing);

#if defined(UMPIRE_ENABLE_BACKTRACE)
  std::size_t prev_size{m_actual_bytes};
#endif

  // If we are destructing, wait for all deallocations to occur
  if (m_is_destructing) {
    // Wait for all pending operations
    for (auto& [resource, chunk] : m_pending_map) {
      chunk->event.wait();
    }
  
    // Deallocate pending chunks (moves to free map)
    while (!m_pending_map.empty()) {
      auto it = m_pending_map.begin();
      auto chunk = it->second;
      do_deallocate(chunk, chunk->data);
    }
  }

  for (auto pair = m_free_map.begin(); pair != m_free_map.end();) {
    auto chunk = (*pair).second;
    UMPIRE_LOG(Debug, "Found chunk @ " << chunk->data);
    if ((chunk->size == chunk->chunk_size) && chunk->free) {
      UMPIRE_LOG(Debug, "Releasing chunk " << chunk->data);

      m_actual_bytes -= chunk->chunk_size;
      m_releasable_bytes -= chunk->chunk_size;
      m_releasable_blocks--;
      m_total_blocks--;

      try {
        aligned_deallocate(chunk->data);
      } catch (...) {
        if (m_is_destructing) {
          //
          // Ignore error in case the underlying vendor API has already shutdown
          //
          UMPIRE_LOG(Error, "Pool is destructing, runtime_error Ignored");
        } else {
          throw;
        }
      }

      chunk->~Chunk(); // manually call destructor
      m_chunk_pool.deallocate(chunk);
      pair = m_free_map.erase(pair);
    } else {
      ++pair;
    }
  }

#if defined(UMPIRE_ENABLE_BACKTRACE)
  if (prev_size > m_actual_bytes) {
    umpire::util::backtrace bt;
    umpire::util::backtracer<>::get_backtrace(bt);
    UMPIRE_LOG(Info, "actual_size:" << m_actual_bytes << " (prev: " << prev_size << ") "
                                    << umpire::util::backtracer<>::print(bt));
  }
#endif
}

std::size_t ResourceAwarePool::getReleasableBlocks() const noexcept
{
  return m_releasable_blocks;
}

std::size_t ResourceAwarePool::getTotalBlocks() const noexcept
{
  return m_total_blocks;
}

std::size_t ResourceAwarePool::getNumPending() const noexcept
{
  return m_pending_map.size();
}

std::size_t ResourceAwarePool::getActualSize() const noexcept
{
  return m_actual_bytes;
}

std::size_t ResourceAwarePool::getAlignedSize() const noexcept
{
  return m_aligned_bytes;
}

std::size_t ResourceAwarePool::getReleasableSize() const noexcept
{
  return m_releasable_bytes;
}

std::size_t ResourceAwarePool::getAlignedHighwaterMark() const noexcept
{
  return m_aligned_highwatermark;
}

std::size_t ResourceAwarePool::getActualHighwaterMark() const noexcept
{
  return m_actual_highwatermark;
}

Platform ResourceAwarePool::getPlatform() noexcept
{
  return m_allocator->getPlatform();
}

camp::resources::Resource ResourceAwarePool::getResource(void* ptr) const
{
  UMPIRE_LOG(Debug, "Calling getResource with (ptr=" << ptr << ")");

  // First, check used chunks
  auto it = m_used_map.find(ptr);
  if (it != m_used_map.end()) {
    auto chunk = it->second;
    return chunk->resource;
  }

  // If not found, check pending chunks
  for (auto& [resource, chunk] : m_pending_map) {
    if (chunk->data == ptr) {
      if (!resource.has_value()) { //since resource is optional
        UMPIRE_LOG(Error, fmt::format("Found ptr {} in pending_map but resource is null", ptr));
        // Returning a default resource for the ResourceAwarePool
        return camp::resources::Host::get_default();
      }
      return *resource;
    }
  }

  UMPIRE_LOG(Warning, fmt::format("The pointer {} is either free or not associated with the ResourceAwarePool."
                                  "Returning the default Host resource...", ptr));

  // Returning a default resource for the ResourceAwarePool
  return camp::resources::Host::get_default();
}

MemoryResourceTraits ResourceAwarePool::getTraits() const noexcept
{
  return m_allocator->getTraits();
}

bool ResourceAwarePool::tracksMemoryUse() const noexcept
{
  return false;
}

std::size_t ResourceAwarePool::getBlocksInPool() const noexcept
{
  return m_used_map.size() + m_free_map.size() + m_pending_map.size();
}

std::size_t ResourceAwarePool::getLargestAvailableBlock() noexcept
{
  if (!m_free_map.size()) {
    return 0;
  }
  return m_free_map.rbegin()->first;
}

void ResourceAwarePool::coalesce() noexcept
{
  UMPIRE_LOG(Debug, "()");

  umpire::event::record([&](auto& event) {
    event.name("coalesce").category(event::category::operation).tag("allocator_name", getName()).tag("replay", "true");
  });

  auto it = m_pending_map.begin();
  while (it != m_pending_map.end()) {
    auto pending_chunk = it->second;
    if (pending_chunk->event.check()) { 
      // pending chunk is finished
      auto next_it = std::next(it);
      do_deallocate(pending_chunk, pending_chunk->data);
      it = next_it;
    } else {
      ++it;
    }
  }

  std::size_t suggested_size{m_should_coalesce(*this)};
  if (0 != suggested_size) {
    UMPIRE_LOG(Debug, "coalesce heuristic true, performing coalesce, suggested size is " << suggested_size);
    do_coalesce(suggested_size);
  }
}

void ResourceAwarePool::do_coalesce(std::size_t suggested_size) noexcept
{
  if (m_free_map.size() > 1) {
    m_is_coalescing = true;
    UMPIRE_LOG(Debug, "()");
    release();
    std::size_t size_post{getActualSize()};

    if (size_post < suggested_size) {
      std::size_t alloc_size{suggested_size - size_post};

      // The coalesce will only ever happen on free chunks, so we use default Host resource
      // Once the chunk is reallocated the resource will be reset.
      camp::resources::Resource r = camp::resources::Host().get_default();

      UMPIRE_LOG(Debug, "coalescing " << alloc_size << " bytes.");
      auto ptr = allocate_resource(alloc_size, r);
      deallocate_resource(ptr, r, alloc_size);
    }
  }
  m_is_coalescing = false;
}

PoolCoalesceHeuristic<ResourceAwarePool> ResourceAwarePool::blocks_releasable(std::size_t nblocks)
{
  return PoolCoalesceHeuristic<ResourceAwarePool>::known(
      [=](const strategy::ResourceAwarePool& pool) {
        return pool.getReleasableBlocks() >= nblocks ? pool.getActualSize() : 0;
      },
      PoolCoalesceHeuristic<ResourceAwarePool>::Kind::blocks_releasable,
      nblocks);
}

PoolCoalesceHeuristic<ResourceAwarePool> ResourceAwarePool::blocks_releasable_hwm(std::size_t nblocks)
{
  return PoolCoalesceHeuristic<ResourceAwarePool>::known(
      [=](const strategy::ResourceAwarePool& pool) {
        return pool.getReleasableBlocks() >= nblocks ? pool.getAlignedHighwaterMark() : 0;
      },
      PoolCoalesceHeuristic<ResourceAwarePool>::Kind::blocks_releasable_hwm,
      nblocks);
}

PoolCoalesceHeuristic<ResourceAwarePool> ResourceAwarePool::percent_releasable(int percentage)
{
  if (percentage < 0 || percentage > 100) {
    UMPIRE_ERROR(runtime_error,
                 fmt::format("Invalid percentage: {}, percentage must be an integer between 0 and 100", percentage));
  }
  if (percentage == 0) {
    return PoolCoalesceHeuristic<ResourceAwarePool>::known(
        [=](const ResourceAwarePool& UMPIRE_UNUSED_ARG(pool)) { return 0; },
        PoolCoalesceHeuristic<ResourceAwarePool>::Kind::percent_releasable,
        percentage);
  } else if (percentage == 100) {
    return PoolCoalesceHeuristic<ResourceAwarePool>::known(
        [=](const strategy::ResourceAwarePool& pool) {
          return pool.getActualSize() == pool.getReleasableSize() ? pool.getActualSize() : 0;
        },
        PoolCoalesceHeuristic<ResourceAwarePool>::Kind::percent_releasable,
        percentage);
  } else {
    float f = (float)((float)percentage / (float)100.0);
    return PoolCoalesceHeuristic<ResourceAwarePool>::known(
        [=](const strategy::ResourceAwarePool& pool) {
          // Calculate threshold in bytes from the percentage
          const std::size_t threshold = static_cast<std::size_t>(f * pool.getActualSize());
          return pool.getReleasableSize() >= threshold ? pool.getActualSize() : 0;
        },
        PoolCoalesceHeuristic<ResourceAwarePool>::Kind::percent_releasable,
        percentage);
  }
}

PoolCoalesceHeuristic<ResourceAwarePool> ResourceAwarePool::percent_releasable_hwm(int percentage)
{
  if (percentage < 0 || percentage > 100) {
    UMPIRE_ERROR(runtime_error,
                 fmt::format("Invalid percentage: {}, percentage must be an integer between 0 and 100", percentage));
  }
  if (percentage == 0) {
    return PoolCoalesceHeuristic<ResourceAwarePool>::known(
        [=](const ResourceAwarePool& UMPIRE_UNUSED_ARG(pool)) { return 0; },
        PoolCoalesceHeuristic<ResourceAwarePool>::Kind::percent_releasable_hwm,
        percentage);
  } else if (percentage == 100) {
    return PoolCoalesceHeuristic<ResourceAwarePool>::known(
        [=](const strategy::ResourceAwarePool& pool) {
          return pool.getActualSize() == pool.getReleasableSize() ? pool.getAlignedHighwaterMark() : 0;
        },
        PoolCoalesceHeuristic<ResourceAwarePool>::Kind::percent_releasable_hwm,
        percentage);
  } else {
    float f = (float)((float)percentage / (float)100.0);
    return PoolCoalesceHeuristic<ResourceAwarePool>::known(
        [=](const strategy::ResourceAwarePool& pool) {
          // Calculate threshold in bytes from the percentage
          const std::size_t threshold = static_cast<std::size_t>(f * pool.getActualSize());
          return pool.getReleasableSize() >= threshold ? pool.getAlignedHighwaterMark() : 0;
        },
        PoolCoalesceHeuristic<ResourceAwarePool>::Kind::percent_releasable_hwm,
        percentage);
  }
}

std::ostream& operator<<(std::ostream& out, umpire::strategy::PoolCoalesceHeuristic<ResourceAwarePool>&)
{
  return out;
}

} // end of namespace strategy
} // end namespace umpire
