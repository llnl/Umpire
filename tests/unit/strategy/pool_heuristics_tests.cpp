//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////

#include <sstream>
#include <string>
#include <utility>

#include "gtest/gtest.h"
#include "umpire/ResourceManager.hpp"
#include "umpire/config.hpp"
#include "umpire/strategy/DynamicPoolList.hpp"
#include "umpire/strategy/PoolCoalesceHeuristic.hpp"
#include "umpire/strategy/QuickPool.hpp"

//
// The heuristic tests assert exact block sizes and counts, which assume the
// requested sizes arrive at the pool unchanged. This does not hold when every
// allocation is padded by an introspection header.
//
#if defined(UMPIRE_ENABLE_INTROSPECTION_HEADER)
#define SKIP_IF_SIZES_ARE_PADDED() \
  GTEST_SKIP() << "Exact pool block accounting does not hold with UMPIRE_ENABLE_INTROSPECTION_HEADER"
#else
#define SKIP_IF_SIZES_ARE_PADDED() ((void)0)
#endif

namespace {
template <typename T>
struct PoolName {
  static constexpr const char* value = "Unknown";
};

template <>
struct PoolName<umpire::strategy::DynamicPoolList> {
  static constexpr const char* value = "DynamicPoolList";
};

template <>
struct PoolName<umpire::strategy::QuickPool> {
  static constexpr const char* value = "QuickPool";
};
} // namespace

template <typename POOL>
struct PoolHeuristicsTest : public ::testing::Test {
  const std::size_t first_block{1024};
  const std::size_t next_block{128};
  const std::size_t alignment{16};

  using myPoolType = POOL;
  using CoalesceHeuristic = umpire::strategy::PoolCoalesceHeuristic<POOL>;
  using TestAllocator = std::pair<umpire::Allocator, POOL*>;

  TestAllocator getAllocator(CoalesceHeuristic h)
  {
    auto& rm = umpire::ResourceManager::getInstance();
    TestAllocator rval;
    static int unique_id{0};
    std::stringstream n;

    n << "Pool_" << std::string{PoolName<myPoolType>::value} << "_" << unique_id++;

    rval.first = rm.makeAllocator<myPoolType>(n.str(), rm.getAllocator("HOST"), first_block, next_block, alignment, h);
    auto strategy = rval.first.getAllocationStrategy();
    rval.second = dynamic_cast<POOL*>(strategy);
    return rval;
  }
};

using PoolTypes = testing::Types<umpire::strategy::DynamicPoolList, umpire::strategy::QuickPool>;

TYPED_TEST_SUITE(PoolHeuristicsTest, PoolTypes, );

TYPED_TEST(PoolHeuristicsTest, PercentReleasable)
{
  using myPoolType = typename TestFixture::myPoolType;
  using TestAllocator = typename TestFixture::TestAllocator;
  TestAllocator a;

  ASSERT_NO_THROW(a = this->getAllocator(myPoolType::percent_releasable(100)););
  ASSERT_NE(a.second, nullptr);

  std::vector<void*> ptrs;
  const int max_blocks{9};

  for (int i{0}; i < max_blocks; i++) {
    ASSERT_NO_THROW(ptrs.push_back(a.first.allocate(this->first_block)););
    ASSERT_EQ(a.second->getReleasableBlocks(), 0);
    ASSERT_EQ(a.second->getTotalBlocks(), i + 1);
  }

  for (int i{max_blocks - 1}; i > 0; i--) {
    ASSERT_NO_THROW(a.first.deallocate(ptrs[i]););
    ASSERT_EQ(a.second->getReleasableBlocks(), max_blocks - i);
  }

  ASSERT_NO_THROW(a.first.deallocate(ptrs[0]););
  ASSERT_EQ(a.second->getReleasableBlocks(), 1);
  ASSERT_EQ(a.second->getTotalBlocks(), 1);
}

TYPED_TEST(PoolHeuristicsTest, PercentReleasableHWM)
{
  SKIP_IF_SIZES_ARE_PADDED();

  using myPoolType = typename TestFixture::myPoolType;
  using TestAllocator = typename TestFixture::TestAllocator;
  TestAllocator a;

  ASSERT_NO_THROW(a = this->getAllocator(myPoolType::percent_releasable_hwm(25)));
  ASSERT_NE(a.second, nullptr);

  std::vector<void*> ptrs;

  // allocate 64 bytes 23 times with first block being 1024 bytes and next_block 128 bytes
  for (int i{0}; i < 23; ++i) {
    ASSERT_NO_THROW(ptrs.push_back(a.first.allocate(64)));
    ASSERT_EQ(a.second->getReleasableBlocks(), 0);
  }

  ASSERT_EQ(a.second->getActualSize(), 1536);
  ASSERT_EQ(a.second->getHighWatermark(), 1472);
  ASSERT_EQ(a.second->getTotalBlocks(), 5);

  // deallocate 7*64 = 448 bytes so that 25% of the pool is relesable and it will coalesce automatically
  for (int i{22}; i > 15; --i) {
    ASSERT_NO_THROW(a.first.deallocate(ptrs[i]););
  }

  ASSERT_EQ(a.second->getActualSize(), a.second->getHighWatermark());
  ASSERT_EQ(a.second->getTotalBlocks(), 2);
  ASSERT_EQ(a.second->getReleasableBlocks(), 1);

  ASSERT_NO_THROW(a.first.release());

  for (int i{16}; i > 0; --i) {
    ASSERT_NO_THROW(a.first.deallocate(ptrs[i - 1]););
  }

  ASSERT_EQ(a.second->getReleasableBlocks(), 1);
  ASSERT_EQ(a.second->getTotalBlocks(), 1);
}

TYPED_TEST(PoolHeuristicsTest, BlocksReleasable)
{
  using myPoolType = typename TestFixture::myPoolType;
  using TestAllocator = typename TestFixture::TestAllocator;
  TestAllocator a;

  ASSERT_NO_THROW(a = this->getAllocator(myPoolType::blocks_releasable(2)););
  ASSERT_NE(a.second, nullptr);

  std::vector<void*> ptrs;
  const int max_blocks{9};

  for (int i{0}; i < max_blocks; i++) {
    ASSERT_NO_THROW(ptrs.push_back(a.first.allocate(this->first_block)););
    ASSERT_EQ(a.second->getReleasableBlocks(), 0);
    ASSERT_EQ(a.second->getTotalBlocks(), i + 1);
  }

  for (int i{max_blocks}; i > 0; i--) {
    ASSERT_NO_THROW(a.first.deallocate(ptrs[i - 1]););
    ASSERT_EQ(a.second->getReleasableBlocks(), 1);
  }

  ASSERT_EQ(a.second->getReleasableBlocks(), 1);
  ASSERT_EQ(a.second->getTotalBlocks(), 1);
}

TYPED_TEST(PoolHeuristicsTest, BlocksReleasableHWM)
{
  SKIP_IF_SIZES_ARE_PADDED();

  using myPoolType = typename TestFixture::myPoolType;
  using TestAllocator = typename TestFixture::TestAllocator;
  TestAllocator a;

  ASSERT_NO_THROW(a = this->getAllocator(myPoolType::blocks_releasable_hwm(2)));
  ASSERT_NE(a.second, nullptr);

  std::vector<void*> ptrs;

  // allocate 64 bytes 23 times with first block being 1024 bytes and next_block 128 bytes
  for (int i{0}; i < 23; ++i) {
    ASSERT_NO_THROW(ptrs.push_back(a.first.allocate(64)));
    ASSERT_EQ(a.second->getReleasableBlocks(), 0);
  }

  ASSERT_EQ(a.second->getActualSize(), 1536);
  ASSERT_EQ(a.second->getHighWatermark(), 1472);
  ASSERT_EQ(a.second->getTotalBlocks(), 5);

  // deallocate 4 times so that two blocks are relesable and they will coalesce automatically
  for (int i{22}; i > 18; --i) {
    ASSERT_NO_THROW(a.first.deallocate(ptrs[i]););
  }

  ASSERT_EQ(a.second->getActualSize(), a.second->getHighWatermark());
  ASSERT_EQ(a.second->getTotalBlocks(), 4);
  ASSERT_EQ(a.second->getReleasableBlocks(), 1);

  for (int i{19}; i > 0; --i) {
    ASSERT_NO_THROW(a.first.deallocate(ptrs[i - 1]););
  }

  ASSERT_EQ(a.second->getReleasableBlocks(), 1);
  ASSERT_EQ(a.second->getTotalBlocks(), 1);
}
