//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#include <cstring>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "umpire/ResourceManager.hpp"
#include "umpire/Umpire.hpp"
#include "umpire/config.hpp"
#include "umpire/strategy/QuickPool.hpp"
#include "umpire/util/AllocationHeader.hpp"
#include "umpire/util/error.hpp"

class IntrospectionHeaderTest : public ::testing::TestWithParam<std::string> {
 public:
  void SetUp() override
  {
    auto& rm = umpire::ResourceManager::getInstance();
    m_allocator = new umpire::Allocator(rm.getAllocator(GetParam()));
  }

  void TearDown() override
  {
    delete m_allocator;
    m_allocator = nullptr;
  }

  umpire::Allocator* m_allocator;
};

TEST_P(IntrospectionHeaderTest, GetSize)
{
  const std::vector<std::size_t> sizes{1, 7, 64, 1021, 4096, 1024 * 1024};

  for (auto size : sizes) {
    void* data = m_allocator->allocate(size);

    ASSERT_EQ(size, m_allocator->getSize(data));

    m_allocator->deallocate(data);
  }
}

TEST_P(IntrospectionHeaderTest, Alignment)
{
  void* data = m_allocator->allocate(17);

  ASSERT_EQ(0, reinterpret_cast<uintptr_t>(data) % alignof(std::max_align_t));

  m_allocator->deallocate(data);
}

TEST_P(IntrospectionHeaderTest, WriteDoesNotCorruptHeader)
{
  const std::size_t size{4096};

  char* data = static_cast<char*>(m_allocator->allocate(size));
  std::memset(data, 0xFF, size);

  ASSERT_EQ(size, m_allocator->getSize(data));

  m_allocator->deallocate(data);
}

TEST_P(IntrospectionHeaderTest, GetAllocatorByPointer)
{
  auto& rm = umpire::ResourceManager::getInstance();

  void* data = m_allocator->allocate(64);

  ASSERT_TRUE(rm.hasAllocator(data));
  ASSERT_EQ(m_allocator->getId(), rm.getAllocator(data).getId());

  m_allocator->deallocate(data);
}

TEST_P(IntrospectionHeaderTest, FindAllocationRecord)
{
  auto& rm = umpire::ResourceManager::getInstance();

  void* data = m_allocator->allocate(128);

  auto record = rm.findAllocationRecord(data);
  ASSERT_EQ(data, record->ptr);
  ASSERT_EQ(128, record->size);
  ASSERT_EQ(m_allocator->getAllocationStrategy(), record->strategy);

  m_allocator->deallocate(data);
}

TEST_P(IntrospectionHeaderTest, OffsetPointerThrows)
{
  auto& rm = umpire::ResourceManager::getInstance();

  char* data = static_cast<char*>(m_allocator->allocate(4096));

  ASSERT_THROW(rm.getSize(data + 128), umpire::runtime_error);

  m_allocator->deallocate(data);
}

TEST_P(IntrospectionHeaderTest, Counters)
{
  const std::size_t initial_size{m_allocator->getCurrentSize()};
  const std::size_t initial_count{m_allocator->getAllocationCount()};

  void* data_one = m_allocator->allocate(1024);
  void* data_two = m_allocator->allocate(2048);

  ASSERT_EQ(initial_size + 3072, m_allocator->getCurrentSize());
  ASSERT_EQ(initial_count + 2, m_allocator->getAllocationCount());
  ASSERT_GE(m_allocator->getHighWatermark(), initial_size + 3072);

  m_allocator->deallocate(data_one);
  m_allocator->deallocate(data_two);

  ASSERT_EQ(initial_size, m_allocator->getCurrentSize());
  ASSERT_EQ(initial_count, m_allocator->getAllocationCount());
}

TEST_P(IntrospectionHeaderTest, ZeroByteAllocation)
{
  void* data = m_allocator->allocate(0);

  ASSERT_NE(nullptr, data);
  ASSERT_EQ(0, m_allocator->getSize(data));

  m_allocator->deallocate(data);
}

TEST_P(IntrospectionHeaderTest, DeallocateWrongAllocatorThrows)
{
  auto& rm = umpire::ResourceManager::getInstance();
  auto other_allocator =
      rm.makeAllocator<umpire::strategy::QuickPool>("wrong_dealloc_pool_" + GetParam(), *m_allocator);

  void* data = m_allocator->allocate(64);

  ASSERT_THROW(other_allocator.deallocate(data), umpire::runtime_error);

  m_allocator->deallocate(data);
}

TEST_P(IntrospectionHeaderTest, Pool)
{
  auto& rm = umpire::ResourceManager::getInstance();
  auto pool = rm.makeAllocator<umpire::strategy::QuickPool>("introspection_header_pool_" + GetParam(), *m_allocator);

  const std::vector<std::size_t> sizes{16, 700, 8192};
  std::vector<void*> allocations;

  for (auto size : sizes) {
    void* data = pool.allocate(size);
    ASSERT_EQ(size, pool.getSize(data));
    allocations.push_back(data);
  }

  ASSERT_EQ(sizes.size(), pool.getAllocationCount());

  for (auto data : allocations) {
    pool.deallocate(data);
  }

  ASSERT_EQ(0, pool.getCurrentSize());
}

TEST_P(IntrospectionHeaderTest, RegisterAllocationThrows)
{
  auto& rm = umpire::ResourceManager::getInstance();

  void* data = m_allocator->allocate(64);

  umpire::util::AllocationRecord record{data, 64, m_allocator->getAllocationStrategy()};
  ASSERT_THROW(rm.registerAllocation(data, record), umpire::runtime_error);
  ASSERT_THROW(rm.deregisterAllocation(data), umpire::runtime_error);

  m_allocator->deallocate(data);
}

TEST_P(IntrospectionHeaderTest, Copy)
{
  auto& rm = umpire::ResourceManager::getInstance();
  const std::size_t size{256};

  char* source = static_cast<char*>(m_allocator->allocate(size));
  char* dest = static_cast<char*>(m_allocator->allocate(size));

  auto host_allocator = rm.getAllocator("HOST");
  char* check = static_cast<char*>(host_allocator.allocate(size));

  for (std::size_t i = 0; i < size; i++) {
    check[i] = static_cast<char>(i % 128);
  }

  rm.copy(source, check);
  rm.copy(dest, source);
  rm.copy(check, dest);

  for (std::size_t i = 0; i < size; i++) {
    ASSERT_EQ(static_cast<char>(i % 128), check[i]);
  }

  m_allocator->deallocate(source);
  m_allocator->deallocate(dest);
  host_allocator.deallocate(check);
}

TEST_P(IntrospectionHeaderTest, Reallocate)
{
  auto& rm = umpire::ResourceManager::getInstance();
  const std::size_t size{64};

  char* data = static_cast<char*>(m_allocator->allocate(size));

  for (std::size_t i = 0; i < size; i++) {
    data[i] = static_cast<char>(i);
  }

  char* bigger = static_cast<char*>(rm.reallocate(data, size * 2));

  ASSERT_EQ(size * 2, m_allocator->getSize(bigger));

  auto host_allocator = rm.getAllocator("HOST");
  char* check = static_cast<char*>(host_allocator.allocate(size));
  rm.copy(check, bigger, size);

  for (std::size_t i = 0; i < size; i++) {
    ASSERT_EQ(static_cast<char>(i), check[i]);
  }

  m_allocator->deallocate(bigger);
  host_allocator.deallocate(check);
}

std::vector<std::string> introspection_header_allocators()
{
  std::vector<std::string> allocators{"HOST"};

#if defined(UMPIRE_ENABLE_UM)
  allocators.push_back("UM");
#endif
#if defined(UMPIRE_ENABLE_PINNED)
  allocators.push_back("PINNED");
#endif

  return allocators;
}

INSTANTIATE_TEST_SUITE_P(IntrospectionHeader, IntrospectionHeaderTest,
                         ::testing::ValuesIn(introspection_header_allocators()));

TEST(IntrospectionHeader, HeaderSizeIsPadded)
{
  ASSERT_GE(umpire::util::allocation_header_size, sizeof(umpire::util::AllocationHeader));
  ASSERT_EQ(0, umpire::util::allocation_header_size % alignof(std::max_align_t));
}
