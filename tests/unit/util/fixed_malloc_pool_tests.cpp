//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#include <algorithm>
#include <cstring>
#include <set>
#include <vector>

#include "gtest/gtest.h"
#include "umpire/util/FixedMallocPool.hpp"
#include "umpire/util/error.hpp"

namespace {

constexpr std::size_t object_bytes = 16;
constexpr std::size_t objects_per_pool = 64;

class FixedMallocPoolTest : public ::testing::Test {
 protected:
  FixedMallocPoolTest() : pool{object_bytes, objects_per_pool}
  {
  }

  umpire::util::FixedMallocPool pool;
};

TEST_F(FixedMallocPoolTest, AllocateDeallocate)
{
  std::vector<void*> ptrs;
  for (int i = 0; i < 8; ++i) {
    void* ptr = pool.allocate(object_bytes);
    ASSERT_NE(ptr, nullptr);
    ptrs.push_back(ptr);
  }

  // All pointers are distinct
  std::set<void*> unique{ptrs.begin(), ptrs.end()};
  ASSERT_EQ(unique.size(), ptrs.size());

  // Each allocation is fully writable without corrupting the others
  for (std::size_t i = 0; i < ptrs.size(); ++i) {
    std::memset(ptrs[i], static_cast<int>(i + 1), object_bytes);
  }
  for (std::size_t i = 0; i < ptrs.size(); ++i) {
    const unsigned char* bytes = static_cast<const unsigned char*>(ptrs[i]);
    for (std::size_t j = 0; j < object_bytes; ++j) {
      ASSERT_EQ(bytes[j], static_cast<unsigned char>(i + 1));
    }
  }

  for (void* ptr : ptrs) {
    pool.deallocate(ptr);
  }
}

TEST_F(FixedMallocPoolTest, LifoReuse)
{
  void* a = pool.allocate(object_bytes);
  void* b = pool.allocate(object_bytes);

  // Recycled slots are preferred over never-used slots, most recently
  // freed first
  pool.deallocate(a);
  pool.deallocate(b);

  ASSERT_EQ(pool.allocate(object_bytes), b);
  ASSERT_EQ(pool.allocate(object_bytes), a);

  pool.deallocate(a);
  pool.deallocate(b);
}

TEST_F(FixedMallocPoolTest, Churn)
{
  // Churning a small live set must recycle slots rather than consuming
  // never-used slots, so the pool never grows. This guards against
  // reintroducing eager initialization of the free list, which touched
  // (and made resident) every page in the pool up front.
  constexpr std::size_t live_set = 4;
  const std::size_t initial_bytes = pool.totalBytes();

  std::vector<void*> ptrs;
  for (std::size_t i = 0; i < 10 * objects_per_pool; ++i) {
    for (std::size_t j = 0; j < live_set; ++j) {
      void* ptr = pool.allocate(object_bytes);
      ASSERT_NE(ptr, nullptr);
      ptrs.push_back(ptr);
    }
    for (void* ptr : ptrs) {
      pool.deallocate(ptr);
    }
    ptrs.clear();

    ASSERT_EQ(pool.numPools(), 1);
    ASSERT_EQ(pool.totalBytes(), initial_bytes);
  }
}

TEST_F(FixedMallocPoolTest, ExhaustionGrowsPool)
{
  std::vector<void*> ptrs;
  for (std::size_t i = 0; i < objects_per_pool; ++i) {
    ptrs.push_back(pool.allocate(object_bytes));
  }

  std::set<void*> unique{ptrs.begin(), ptrs.end()};
  ASSERT_EQ(unique.size(), objects_per_pool);
  ASSERT_EQ(pool.numPools(), 1);

  // One more allocation than the pool holds triggers growth
  void* extra = pool.allocate(object_bytes);
  ASSERT_NE(extra, nullptr);
  ASSERT_EQ(pool.numPools(), 2);

  pool.deallocate(extra);
  for (void* ptr : ptrs) {
    pool.deallocate(ptr);
  }

  // Freed slots are recycled from the existing pools
  for (std::size_t i = 0; i < objects_per_pool + 1; ++i) {
    ASSERT_NE(pool.allocate(object_bytes), nullptr);
  }
  ASSERT_EQ(pool.numPools(), 2);
}

TEST(FixedMallocPoolSmallObjectTest, IntSizedObjects)
{
  // MemoryMap<int> instantiates FixedMallocPool with sizeof(int); the
  // free list must fit in a slot that small
  umpire::util::FixedMallocPool small_pool{sizeof(int), objects_per_pool};

  void* a = small_pool.allocate(sizeof(int));
  void* b = small_pool.allocate(sizeof(int));
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);
  ASSERT_NE(a, b);

  small_pool.deallocate(a);
  small_pool.deallocate(b);

  ASSERT_EQ(small_pool.allocate(sizeof(int)), b);
  ASSERT_EQ(small_pool.allocate(sizeof(int)), a);

  small_pool.deallocate(a);
  small_pool.deallocate(b);
}

TEST_F(FixedMallocPoolTest, DeallocateUnknownPointer)
{
  int not_from_pool;
  ASSERT_THROW(pool.deallocate(&not_from_pool), umpire::runtime_error);
}

} // namespace
