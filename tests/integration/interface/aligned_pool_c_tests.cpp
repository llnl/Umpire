//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#include "gtest/gtest.h"
#include "umpire/config.hpp"
#include "umpire/interface/c_fortran/umpire.h"
#include <cstdint>
#include <string>

static int unique_name = 0;

// Helper function to check alignment
static inline void test_alignment(uintptr_t p, size_t align)
{
  ASSERT_EQ(0, p % align) << "Pointer " << reinterpret_cast<void*>(p)
                          << " is not aligned to " << align << " bytes";
}

const char* pool_names[] = {"HOST"
#if defined(UMPIRE_ENABLE_PINNED)
                            ,
                            "PINNED"
#endif
#if defined(UMPIRE_ENABLE_DEVICE)
                            ,
                            "DEVICE"
#endif
#if defined(UMPIRE_ENABLE_UM)
                            ,
                            "UM"
#endif
};

class AlignedListPoolAllocatorCTest : public ::testing::TestWithParam<const char*> {
 public:
  virtual void SetUp()
  {
    std::string pool_name = std::string{GetParam()} + "_c_aligned_list_pool" + std::to_string(unique_name++);

    umpire_resourcemanager rm;
    umpire_resourcemanager_get_instance(&rm);
    umpire_resourcemanager_get_allocator_by_name(&rm, GetParam(), &m_allocator);

    umpire_resourcemanager_make_allocator_list_pool_aligned(
        &rm, pool_name.c_str(), m_allocator, m_pool_init, m_small, m_alignment, &m_pool);
  }

  virtual void TearDown()
  {
    umpire_allocator_delete(&m_allocator);
    umpire_allocator_delete(&m_pool);
  }

  umpire_allocator m_allocator;
  umpire_allocator m_pool;

  const std::size_t m_pool_init = 1024 * 1024 * 16;  // 16 MB initial pool
  const std::size_t m_big = 1024 * 1024;
  const std::size_t m_small = 64;
  const std::size_t m_nothing = 0;
  const std::size_t m_alignment = 64;  // 64-byte alignment
};

TEST_P(AlignedListPoolAllocatorCTest, AllocateWithAlignment)
{
  double* data = (double*)umpire_allocator_allocate(&m_pool, m_big * sizeof(double));
  ASSERT_NE(nullptr, data);

  // Check that the allocation is properly aligned
  test_alignment(reinterpret_cast<uintptr_t>(data), m_alignment);

  umpire_allocator_deallocate(&m_pool, data);
}

TEST_P(AlignedListPoolAllocatorCTest, MultipleAllocationsAligned)
{
  // Allocate multiple blocks and check each is aligned
  void* ptr1 = umpire_allocator_allocate(&m_pool, 128);
  void* ptr2 = umpire_allocator_allocate(&m_pool, 256);
  void* ptr3 = umpire_allocator_allocate(&m_pool, 512);

  ASSERT_NE(nullptr, ptr1);
  ASSERT_NE(nullptr, ptr2);
  ASSERT_NE(nullptr, ptr3);

  test_alignment(reinterpret_cast<uintptr_t>(ptr1), m_alignment);
  test_alignment(reinterpret_cast<uintptr_t>(ptr2), m_alignment);
  test_alignment(reinterpret_cast<uintptr_t>(ptr3), m_alignment);

  umpire_allocator_deallocate(&m_pool, ptr1);
  umpire_allocator_deallocate(&m_pool, ptr2);
  umpire_allocator_deallocate(&m_pool, ptr3);
}

INSTANTIATE_TEST_SUITE_P(AlignedListPools, AlignedListPoolAllocatorCTest, ::testing::ValuesIn(pool_names));

class AlignedQuickPoolAllocatorCTest : public ::testing::TestWithParam<const char*> {
 public:
  virtual void SetUp()
  {
    std::string pool_name = std::string{GetParam()} + "_c_aligned_quick_pool" + std::to_string(unique_name++);

    umpire_resourcemanager rm;
    umpire_resourcemanager_get_instance(&rm);
    umpire_resourcemanager_get_allocator_by_name(&rm, GetParam(), &m_allocator);

    umpire_resourcemanager_make_allocator_quick_pool_aligned(
        &rm, pool_name.c_str(), m_allocator, m_pool_init, m_small, m_alignment, &m_pool);
  }

  virtual void TearDown()
  {
    umpire_allocator_delete(&m_allocator);
    umpire_allocator_delete(&m_pool);
  }

  umpire_allocator m_allocator;
  umpire_allocator m_pool;

  const std::size_t m_pool_init = 1024 * 1024 * 16;  // 16 MB initial pool
  const std::size_t m_big = 1024 * 1024;
  const std::size_t m_small = 64;
  const std::size_t m_nothing = 0;
  const std::size_t m_alignment = 128;  // 128-byte alignment
};

TEST_P(AlignedQuickPoolAllocatorCTest, AllocateWithAlignment)
{
  double* data = (double*)umpire_allocator_allocate(&m_pool, m_big * sizeof(double));
  ASSERT_NE(nullptr, data);

  // Check that the allocation is properly aligned
  test_alignment(reinterpret_cast<uintptr_t>(data), m_alignment);

  umpire_allocator_deallocate(&m_pool, data);
}

TEST_P(AlignedQuickPoolAllocatorCTest, MultipleAllocationsAligned)
{
  // Allocate multiple blocks and check each is aligned
  void* ptr1 = umpire_allocator_allocate(&m_pool, 128);
  void* ptr2 = umpire_allocator_allocate(&m_pool, 256);
  void* ptr3 = umpire_allocator_allocate(&m_pool, 512);

  ASSERT_NE(nullptr, ptr1);
  ASSERT_NE(nullptr, ptr2);
  ASSERT_NE(nullptr, ptr3);

  test_alignment(reinterpret_cast<uintptr_t>(ptr1), m_alignment);
  test_alignment(reinterpret_cast<uintptr_t>(ptr2), m_alignment);
  test_alignment(reinterpret_cast<uintptr_t>(ptr3), m_alignment);

  umpire_allocator_deallocate(&m_pool, ptr1);
  umpire_allocator_deallocate(&m_pool, ptr2);
  umpire_allocator_deallocate(&m_pool, ptr3);
}

INSTANTIATE_TEST_SUITE_P(AlignedQuickPools, AlignedQuickPoolAllocatorCTest, ::testing::ValuesIn(pool_names));

class AlignedResourceAwarePoolAllocatorCTest : public ::testing::TestWithParam<const char*> {
 public:
  virtual void SetUp()
  {
    std::string pool_name = std::string{GetParam()} + "_c_aligned_rap" + std::to_string(unique_name++);

    umpire_resourcemanager rm;
    umpire_resourcemanager_get_instance(&rm);
    umpire_resourcemanager_get_allocator_by_name(&rm, GetParam(), &m_allocator);

    umpire_resourcemanager_make_allocator_resource_aware_pool_aligned(
        &rm, pool_name.c_str(), m_allocator, m_pool_init, m_small, m_alignment, &m_pool);
  }

  virtual void TearDown()
  {
    umpire_allocator_delete(&m_allocator);
    umpire_allocator_delete(&m_pool);
  }

  umpire_allocator m_allocator;
  umpire_allocator m_pool;

  const std::size_t m_pool_init = 1024 * 1024 * 16;  // 16 MB initial pool
  const std::size_t m_big = 1024 * 1024;
  const std::size_t m_small = 64;
  const std::size_t m_nothing = 0;
  const std::size_t m_alignment = 256;  // 256-byte alignment
};

TEST_P(AlignedResourceAwarePoolAllocatorCTest, AllocateWithAlignment)
{
  double* data = (double*)umpire_allocator_allocate(&m_pool, m_big * sizeof(double));
  ASSERT_NE(nullptr, data);

  // Check that the allocation is properly aligned
  test_alignment(reinterpret_cast<uintptr_t>(data), m_alignment);

  umpire_allocator_deallocate(&m_pool, data);
}

TEST_P(AlignedResourceAwarePoolAllocatorCTest, MultipleAllocationsAligned)
{
  // Allocate multiple blocks and check each is aligned
  void* ptr1 = umpire_allocator_allocate(&m_pool, 128);
  void* ptr2 = umpire_allocator_allocate(&m_pool, 256);
  void* ptr3 = umpire_allocator_allocate(&m_pool, 512);

  ASSERT_NE(nullptr, ptr1);
  ASSERT_NE(nullptr, ptr2);
  ASSERT_NE(nullptr, ptr3);

  test_alignment(reinterpret_cast<uintptr_t>(ptr1), m_alignment);
  test_alignment(reinterpret_cast<uintptr_t>(ptr2), m_alignment);
  test_alignment(reinterpret_cast<uintptr_t>(ptr3), m_alignment);

  umpire_allocator_deallocate(&m_pool, ptr1);
  umpire_allocator_deallocate(&m_pool, ptr2);
  umpire_allocator_deallocate(&m_pool, ptr3);
}

INSTANTIATE_TEST_SUITE_P(AlignedResourceAwarePools, AlignedResourceAwarePoolAllocatorCTest, ::testing::ValuesIn(pool_names));

// Test untracked versions
class AlignedListPoolUntrackedAllocatorCTest : public ::testing::TestWithParam<const char*> {
 public:
  virtual void SetUp()
  {
    std::string pool_name = std::string{GetParam()} + "_c_aligned_list_pool_untracked" + std::to_string(unique_name++);

    umpire_resourcemanager rm;
    umpire_resourcemanager_get_instance(&rm);
    umpire_resourcemanager_get_allocator_by_name(&rm, GetParam(), &m_allocator);

    umpire_resourcemanager_make_allocator_list_pool_aligned_untracked(
        &rm, pool_name.c_str(), m_allocator, m_pool_init, m_small, m_alignment, &m_pool);
  }

  virtual void TearDown()
  {
    umpire_allocator_delete(&m_allocator);
    umpire_allocator_delete(&m_pool);
  }

  umpire_allocator m_allocator;
  umpire_allocator m_pool;

  const std::size_t m_pool_init = 1024 * 1024 * 16;  // 16 MB initial pool
  const std::size_t m_big = 1024 * 1024;
  const std::size_t m_small = 64;
  const std::size_t m_nothing = 0;
  const std::size_t m_alignment = 64;
};

TEST_P(AlignedListPoolUntrackedAllocatorCTest, AllocateWithAlignment)
{
  double* data = (double*)umpire_allocator_allocate(&m_pool, m_big * sizeof(double));
  ASSERT_NE(nullptr, data);

  test_alignment(reinterpret_cast<uintptr_t>(data), m_alignment);

  umpire_allocator_deallocate(&m_pool, data);
}

INSTANTIATE_TEST_SUITE_P(AlignedListPoolsUntracked, AlignedListPoolUntrackedAllocatorCTest, ::testing::ValuesIn(pool_names));
