//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#include "umpire/strategy/DynamicPoolList.hpp"

#include "umpire/Allocator.hpp"
#include "umpire/ResourceManager.hpp"
#include "umpire/strategy/PoolCoalesceHeuristic.hpp"
#include "umpire/util/Macros.hpp"

namespace umpire {
namespace strategy {

DynamicPoolList::DynamicPoolList(const std::string& name, int id, Allocator allocator,
                                 const std::size_t first_minimum_pool_allocation_size,
                                 const std::size_t next_minimum_pool_allocation_size, const std::size_t alignment,
                                 PoolCoalesceHeuristic<DynamicPoolList> should_coalesce) noexcept
    : AllocationStrategy{name, id, allocator.getAllocationStrategy(), "DynamicPoolList"},
      m_allocator{allocator.getAllocationStrategy()},
      dpa{m_allocator, first_minimum_pool_allocation_size, next_minimum_pool_allocation_size, alignment},
      m_should_coalesce{should_coalesce}
{
}

void* DynamicPoolList::allocate(size_t bytes)
{
  UMPIRE_LOG(Debug, "(bytes=" << bytes << ")");

  void* ptr = dpa.allocate(bytes);
  return ptr;
}

void DynamicPoolList::deallocate(void* ptr, std::size_t UMPIRE_UNUSED_ARG(size))
{
  UMPIRE_LOG(Debug, "(ptr=" << ptr << ")");
  dpa.deallocate(ptr);

  std::size_t suggested_size{m_should_coalesce(*this)};
  if (0 != suggested_size) {
    UMPIRE_LOG(Debug,
               "Heuristic returned true, "
               "performing coalesce operation for "
                   << this << "\n");
    dpa.coalesce(suggested_size);
  }
}

void DynamicPoolList::release()
{
  UMPIRE_LOG(Debug, "()");
  dpa.release();
}

std::size_t DynamicPoolList::getReleasableBlocks() const noexcept
{
  return dpa.getReleasableBlocks();
}

std::size_t DynamicPoolList::getTotalBlocks() const noexcept
{
  return dpa.getTotalBlocks();
}

std::size_t DynamicPoolList::getActualSize() const noexcept
{
  std::size_t ActualSize = dpa.getActualSize();
  UMPIRE_LOG(Debug, "() returning " << ActualSize);
  return ActualSize;
}

std::size_t DynamicPoolList::getActualHighwaterMark() const noexcept
{
  return dpa.getActualHighwaterMark();
}

std::size_t DynamicPoolList::getAlignedSize() const noexcept
{
  return dpa.getAlignedSize();
}

std::size_t DynamicPoolList::getAlignedHighwaterMark() const noexcept
{
  return dpa.getAlignedHighwaterMark();
}

std::size_t DynamicPoolList::getReleasableSize() const noexcept
{
  std::size_t SparseBlockSize = dpa.getReleasableSize();
  UMPIRE_LOG(Debug, "() returning " << SparseBlockSize);
  return SparseBlockSize;
}

std::size_t DynamicPoolList::getBlocksInPool() const noexcept
{
  std::size_t BlocksInPool = dpa.getBlocksInPool();
  UMPIRE_LOG(Debug, "() returning " << BlocksInPool);
  return BlocksInPool;
}

std::size_t DynamicPoolList::getLargestAvailableBlock() const noexcept
{
  std::size_t LargestAvailableBlock = dpa.getLargestAvailableBlock();
  UMPIRE_LOG(Debug, "() returning " << LargestAvailableBlock);
  return LargestAvailableBlock;
}

Platform DynamicPoolList::getPlatform() noexcept
{
  return m_allocator->getPlatform();
}

MemoryResourceTraits DynamicPoolList::getTraits() const noexcept
{
  return m_allocator->getTraits();
}

bool DynamicPoolList::tracksMemoryUse() const noexcept
{
  return true;
}

void DynamicPoolList::coalesce() noexcept
{
  UMPIRE_LOG(Debug, "()");
  umpire::event::record([&](auto& event) {
    event.name("coalesce").category(event::category::operation).tag("allocator_name", getName()).tag("replay", "true");
  });

  std::size_t suggested_size{m_should_coalesce(*this)};
  if (0 != suggested_size) {
    UMPIRE_LOG(Debug, "coalesce heuristic true, performing coalesce, suggested size is " << suggested_size);
    dpa.coalesce(suggested_size);
  }
}

PoolCoalesceHeuristic<DynamicPoolList> DynamicPoolList::blocks_releasable(std::size_t nblocks)
{
  return PoolCoalesceHeuristic<DynamicPoolList>::known(
      [=](const strategy::DynamicPoolList& pool) {
        return pool.getReleasableBlocks() >= nblocks ? pool.getActualSize() : 0;
      },
      PoolCoalesceHeuristic<DynamicPoolList>::Kind::blocks_releasable,
      nblocks);
}

PoolCoalesceHeuristic<DynamicPoolList> DynamicPoolList::blocks_releasable_hwm(std::size_t nblocks)
{
  return PoolCoalesceHeuristic<DynamicPoolList>::known(
      [=](const strategy::DynamicPoolList& pool) {
        return pool.getReleasableBlocks() >= nblocks ? pool.getAlignedHighwaterMark() : 0;
      },
      PoolCoalesceHeuristic<DynamicPoolList>::Kind::blocks_releasable_hwm,
      nblocks);
}

PoolCoalesceHeuristic<DynamicPoolList> DynamicPoolList::percent_releasable(int percentage)
{
  if (percentage < 0 || percentage > 100) {
    UMPIRE_ERROR(runtime_error,
                 fmt::format("Invalid percentage {}, percentage must be an integer between 0 and 100", percentage));
  }

  if (percentage == 0) {
    return PoolCoalesceHeuristic<DynamicPoolList>::known(
        [=](const DynamicPoolList& UMPIRE_UNUSED_ARG(pool)) { return 0; },
        PoolCoalesceHeuristic<DynamicPoolList>::Kind::percent_releasable,
        percentage);
  } else if (percentage == 100) {
    return PoolCoalesceHeuristic<DynamicPoolList>::known(
        [=](const strategy::DynamicPoolList& pool) { return pool.getCurrentSize() == 0 ? pool.getActualSize() : 0; },
        PoolCoalesceHeuristic<DynamicPoolList>::Kind::percent_releasable,
        percentage);
  } else {
    float f = (float)((float)percentage / (float)100.0);
    return PoolCoalesceHeuristic<DynamicPoolList>::known(
        [=](const strategy::DynamicPoolList& pool) {
          // Calculate threshold in bytes from the percentage
          const std::size_t threshold = static_cast<std::size_t>(f * pool.getActualSize());
          return pool.getReleasableSize() >= threshold ? pool.getActualSize() : 0;
        },
        PoolCoalesceHeuristic<DynamicPoolList>::Kind::percent_releasable,
        percentage);
  }
}

PoolCoalesceHeuristic<DynamicPoolList> DynamicPoolList::percent_releasable_hwm(int percentage)
{
  if (percentage < 0 || percentage > 100) {
    UMPIRE_ERROR(runtime_error,
                 fmt::format("Invalid percentage {}, percentage must be an integer between 0 and 100", percentage));
  }

  if (percentage == 0) {
    return PoolCoalesceHeuristic<DynamicPoolList>::known(
        [=](const DynamicPoolList& UMPIRE_UNUSED_ARG(pool)) { return 0; },
        PoolCoalesceHeuristic<DynamicPoolList>::Kind::percent_releasable_hwm,
        percentage);
  } else if (percentage == 100) {
    return PoolCoalesceHeuristic<DynamicPoolList>::known(
        [=](const strategy::DynamicPoolList& pool) {
          return pool.getCurrentSize() == 0 ? pool.getAlignedHighwaterMark() : 0;
        },
        PoolCoalesceHeuristic<DynamicPoolList>::Kind::percent_releasable_hwm,
        percentage);
  } else {
    float f = (float)((float)percentage / (float)100.0);
    return PoolCoalesceHeuristic<DynamicPoolList>::known(
        [=](const strategy::DynamicPoolList& pool) {
          // Calculate threshold in bytes from the percentage
          const std::size_t threshold = static_cast<std::size_t>(f * pool.getActualSize());
          return pool.getReleasableSize() >= threshold ? pool.getAlignedHighwaterMark() : 0;
        },
        PoolCoalesceHeuristic<DynamicPoolList>::Kind::percent_releasable_hwm,
        percentage);
  }
}

std::ostream& operator<<(std::ostream& out, PoolCoalesceHeuristic<DynamicPoolList>&)
{
  return out;
}

} // end of namespace strategy
} // end of namespace umpire
