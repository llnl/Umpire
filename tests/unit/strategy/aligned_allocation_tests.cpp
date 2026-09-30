//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////

#include <limits>

#include "gtest/gtest.h"
#include "umpire/Allocator.hpp"
#include "umpire/ResourceManager.hpp"
#include "umpire/strategy/AlignedAllocator.hpp"
#include "umpire/strategy/DynamicPoolList.hpp"
#include "umpire/strategy/QuickPool.hpp"
#include "umpire/strategy/ResourceAwarePool.hpp"
#include "umpire/util/error.hpp"

namespace {

constexpr std::size_t s_first_block_size{1024};
constexpr std::size_t s_next_block_size{1024};

template <typename Pool>
void check_rounding_overflow(const std::string& name, std::size_t alignment)
{
  auto& rm = umpire::ResourceManager::getInstance();
  auto pool =
      rm.makeAllocator<Pool>(name, rm.getAllocator("HOST"), s_first_block_size, s_next_block_size, alignment);

  void* ptr{nullptr};
  ASSERT_NO_THROW(ptr = pool.allocate(64));

  ASSERT_THROW(pool.allocate(std::numeric_limits<std::size_t>::max()), umpire::runtime_error);
  ASSERT_THROW(pool.allocate(std::numeric_limits<std::size_t>::max() - 1), umpire::runtime_error);
  ASSERT_THROW(pool.allocate(std::numeric_limits<std::size_t>::max() - (alignment - 1)), umpire::runtime_error);

  ASSERT_NO_THROW(pool.deallocate(ptr));
}

void check_aligned_allocator_overflow(const std::string& name, std::size_t alignment)
{
  auto& rm = umpire::ResourceManager::getInstance();
  auto allocator = rm.makeAllocator<umpire::strategy::AlignedAllocator>(name, rm.getAllocator("HOST"), alignment);

  void* ptr{nullptr};
  ASSERT_NO_THROW(ptr = allocator.allocate(64));

  ASSERT_THROW(allocator.allocate(std::numeric_limits<std::size_t>::max()), umpire::runtime_error);
  ASSERT_THROW(allocator.allocate(std::numeric_limits<std::size_t>::max() - 1), umpire::runtime_error);
  ASSERT_THROW(allocator.allocate(std::numeric_limits<std::size_t>::max() - (alignment - 1)), umpire::runtime_error);

  ASSERT_NO_THROW(allocator.deallocate(ptr));
}

} // namespace

TEST(AlignedAllocationTest, QuickPoolRoundingOverflow)
{
  check_rounding_overflow<umpire::strategy::QuickPool>("quick_pool_rounding_overflow", 16);
}

TEST(AlignedAllocationTest, QuickPoolRoundingOverflowLargeAlignment)
{
  check_rounding_overflow<umpire::strategy::QuickPool>("quick_pool_rounding_overflow_large_alignment", 256);
}

TEST(AlignedAllocationTest, DynamicPoolListRoundingOverflow)
{
  check_rounding_overflow<umpire::strategy::DynamicPoolList>("dynamic_pool_list_rounding_overflow", 16);
}

TEST(AlignedAllocationTest, DynamicPoolListRoundingOverflowLargeAlignment)
{
  check_rounding_overflow<umpire::strategy::DynamicPoolList>("dynamic_pool_list_rounding_overflow_large_alignment",
                                                             256);
}

TEST(AlignedAllocationTest, ResourceAwarePoolRoundingOverflow)
{
  check_rounding_overflow<umpire::strategy::ResourceAwarePool>("resource_aware_pool_rounding_overflow", 16);
}

TEST(AlignedAllocationTest, AlignedAllocatorOverflow)
{
  check_aligned_allocator_overflow("aligned_allocator_overflow", 16);
}

TEST(AlignedAllocationTest, AlignedAllocatorOverflowLargeAlignment)
{
  check_aligned_allocator_overflow("aligned_allocator_overflow_large_alignment", 256);
}
