//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#include "gtest/gtest.h"
#include "umpire/Allocator.hpp"
#include "umpire/ResourceManager.hpp"
#include "umpire/Umpire.hpp"
#include "umpire/config.hpp"
#include "umpire/resource/MemoryResourceTypes.hpp"
#include "umpire/strategy/SizeLimiter.hpp"

class AllocatorTest : public ::testing::TestWithParam<std::string> {
 public:
  virtual void SetUp()
  {
    auto& rm = umpire::ResourceManager::getInstance();
    ASSERT_TRUE(rm.isAllocator(GetParam()));
    m_allocator = new umpire::Allocator(rm.getAllocator(GetParam()));
  }

  virtual void TearDown()
  {
    delete m_allocator;
  }

  umpire::Allocator* m_allocator;

  const std::size_t m_big = 64;
  const std::size_t m_small = 8;
  const std::size_t m_nothing = 0;
};

TEST_P(AllocatorTest, AllocateDeallocateBig)
{
  double* data = static_cast<double*>(m_allocator->allocate(m_big * sizeof(double)));

  ASSERT_NE(nullptr, data);

  m_allocator->deallocate(data);
}

TEST_P(AllocatorTest, GetParentCheck)
{
  // Parent of default allocator should be nullptr
  ASSERT_EQ(nullptr, m_allocator->getParent());
}

TEST_P(AllocatorTest, AllocateDeallocateSmall)
{
  double* data = static_cast<double*>(m_allocator->allocate(m_small * sizeof(double)));

  ASSERT_NE(nullptr, data);

  m_allocator->deallocate(data);
}

TEST_P(AllocatorTest, AllocateDeallocateNothing)
{
  // CUDA doesn't support allocating 0 bytes
  if (m_allocator->getPlatform() == umpire::Platform::cuda || m_allocator->getPlatform() == umpire::Platform::hip) {
    SUCCEED();
  } else {
    double* data = static_cast<double*>(m_allocator->allocate(m_nothing * sizeof(double)));

    ASSERT_NE(nullptr, data);

    m_allocator->deallocate(data);
  }
}

TEST_P(AllocatorTest, DeallocateNullptr)
{
  double* data = nullptr;

  ASSERT_NO_THROW(m_allocator->deallocate(data));

  SUCCEED();
}

TEST_P(AllocatorTest, GetSize)
{
  const std::size_t size = m_big * sizeof(double);

  double* data = static_cast<double*>(m_allocator->allocate(size));

  ASSERT_EQ(size, m_allocator->getSize(data));

  m_allocator->deallocate(data);

  ASSERT_ANY_THROW(m_allocator->getSize(data));
}

TEST_P(AllocatorTest, GetName)
{
  ASSERT_TRUE(GetParam().find(m_allocator->getName()) != std::string::npos);
}

TEST_P(AllocatorTest, GetById)
{
  auto& rm = umpire::ResourceManager::getInstance();

  int id = m_allocator->getId();
  ASSERT_GE(id, 0);

  auto allocator_by_id = rm.getAllocator(id);

  ASSERT_EQ(m_allocator->getAllocationStrategy(), allocator_by_id.getAllocationStrategy());

  ASSERT_THROW(rm.getAllocator(-25), umpire::runtime_error);
}

TEST_P(AllocatorTest, get_allocator_records)
{
#if defined(UMPIRE_ENABLE_INTROSPECTION_HEADER)
  GTEST_SKIP() << "Allocation records cannot be enumerated with UMPIRE_ENABLE_INTROSPECTION_HEADER";
#endif

  double* data = static_cast<double*>(m_allocator->allocate(m_small * sizeof(double)));

  auto records = umpire::get_allocator_records(*m_allocator);

  for (const auto& r : records) {
    std::cout << r.ptr << " " << r.size << " " << r.strategy << std::endl;
  }

  EXPECT_EQ(records.size(), 1);

  m_allocator->deallocate(data);
}

TEST_P(AllocatorTest, getCurrentSize)
{
  ASSERT_EQ(m_allocator->getCurrentSize(), 0);

  void* data = m_allocator->allocate(128);

  ASSERT_EQ(m_allocator->getCurrentSize(), 128);

  m_allocator->deallocate(data);
}

TEST_P(AllocatorTest, getActualSize)
{
  ASSERT_EQ(m_allocator->getActualSize(), 0);

  void* data = m_allocator->allocate(128);

  ASSERT_EQ(m_allocator->getActualSize(), 128);

  m_allocator->deallocate(data);
}

TEST_P(AllocatorTest, getStrategyName)
{
  ASSERT_EQ(m_allocator->getStrategyName(), "MemoryResource");
}

std::vector<std::string> allocator_strings()
{
  std::vector<std::string> allocators;
  allocators.push_back("HOST");
#if defined(UMPIRE_ENABLE_DEVICE)
  allocators.push_back("DEVICE");
  auto& rm = umpire::ResourceManager::getInstance();
  for (int id = 0; id < rm.getNumDevices(); id++) {
    allocators.push_back(std::string{"DEVICE::" + std::to_string(id)});
  }
#endif
#if defined(UMPIRE_ENABLE_UM)
  allocators.push_back("UM");
#endif
#if defined(UMPIRE_ENABLE_CONST)
  allocators.push_back("DEVICE_CONST");
#endif
#if defined(UMPIRE_ENABLE_PINNED)
  allocators.push_back("PINNED");
#endif

#if defined(UMPIRE_ENABLE_HIP_COHERENCE_GRANULARITY)
#if defined(UMPIRE_ENABLE_DEVICE)
  int coherence{0};
  auto error = hipDeviceGetAttribute(&coherence, hipDeviceAttributeFineGrainSupport, 0);
  UMPIRE_USE_VAR(error);
  const bool coherence_enabled{coherence == 1};

  if (coherence_enabled) {
    allocators.push_back("DEVICE::FINE");
    allocators.push_back("DEVICE::COARSE");
    allocators.push_back("DEVICE::0::COARSE");
  }
#endif
#if defined(UMPIRE_ENABLE_UM)
  if (coherence_enabled) {
    allocators.push_back("UM::FINE");
    allocators.push_back("UM::COARSE");
  }
#endif
#if defined(UMPIRE_ENABLE_PINNED)
  if (coherence_enabled) {
    allocators.push_back("PINNED::COARSE");
    allocators.push_back("PINNED::FINE");
  }
#endif
#endif

  return allocators;
}

INSTANTIATE_TEST_SUITE_P(Allocators, AllocatorTest, ::testing::ValuesIn(allocator_strings()));

TEST(Allocator, registerAllocator)
{
  auto& rm = umpire::ResourceManager::getInstance();

  for (const std::string& allocator_name : allocator_strings()) {
    const std::string allocator_copy_name = allocator_name + std::string{"_copy"};

    rm.addAlias(allocator_copy_name, rm.getAllocator(allocator_name));

    ASSERT_EQ(rm.getAllocator(allocator_name).getAllocationStrategy(),
              rm.getAllocator(allocator_copy_name).getAllocationStrategy());

    ASSERT_ANY_THROW(rm.addAlias(allocator_name, rm.getAllocator(allocator_copy_name)));

    int id = rm.getAllocator(allocator_name).getId();

    ASSERT_TRUE(rm.isAllocator(allocator_name));
    ASSERT_TRUE(rm.isAllocator(id));
  }

  ASSERT_FALSE(rm.isAllocator("BANANAS"));
}

TEST(Allocator, TryGetAllocator)
{
  auto& rm = umpire::ResourceManager::getInstance();

  auto host_allocator = rm.tryGetAllocator("HOST");
  ASSERT_TRUE(host_allocator.has_value());
  EXPECT_EQ(std::string{"HOST"}, host_allocator->getName());

  auto bad_allocator = rm.tryGetAllocator("BANANAS");
  EXPECT_FALSE(bad_allocator.has_value());
}

TEST(Allocator, GetSetDefault)
{
  auto& rm = umpire::ResourceManager::getInstance();

  ASSERT_NO_THROW(auto alloc = rm.getDefaultAllocator(); UMPIRE_USE_VAR(alloc););

  ASSERT_NO_THROW(rm.setDefaultAllocator(rm.getDefaultAllocator()););
}

class AllocatorByResourceTest : public ::testing::TestWithParam<umpire::resource::MemoryResourceType> {
 public:
  virtual void SetUp()
  {
    auto& rm = umpire::ResourceManager::getInstance();
    m_allocator = new umpire::Allocator(rm.getAllocator(GetParam()));
  }

  virtual void TearDown()
  {
    delete m_allocator;
  }

  umpire::Allocator* m_allocator;

  const std::size_t m_big = 64;
  const std::size_t m_small = 8;
  const std::size_t m_nothing = 0;
};

TEST_P(AllocatorByResourceTest, AllocateDeallocate)
{
  double* data = static_cast<double*>(m_allocator->allocate(m_big * sizeof(double)));

  ASSERT_NE(nullptr, data);

  m_allocator->deallocate(data);
}

TEST_P(AllocatorByResourceTest, AllocateDuplicateDeallocate)
{
  double* data = static_cast<double*>(m_allocator->allocate(m_big * sizeof(double)));

  ASSERT_NE(nullptr, data);

  ASSERT_NO_THROW(m_allocator->deallocate(data));

  ASSERT_THROW(m_allocator->deallocate(data), umpire::runtime_error);
}

const umpire::resource::MemoryResourceType resource_types[] = {umpire::resource::Host
#if defined(UMPIRE_ENABLE_DEVICE)
                                                               ,
                                                               umpire::resource::Device
#endif
#if defined(UMPIRE_ENABLE_UM)
                                                               ,
                                                               umpire::resource::Unified
#endif
#if defined(UMPIRE_ENABLE_PINNED)
                                                               ,
                                                               umpire::resource::Pinned
#endif
#if defined(UMPIRE_ENABLE_CONST)
                                                               ,
                                                               umpire::resource::Constant
#endif
};

INSTANTIATE_TEST_SUITE_P(Resources, AllocatorByResourceTest, ::testing::ValuesIn(resource_types));

TEST(Allocation, DeallocateDifferent)
{
  auto& rm = umpire::ResourceManager::getInstance();

  auto alloc_one = rm.getAllocator("HOST");
  auto alloc_two = rm.makeAllocator<umpire::strategy::SizeLimiter>("Limiter", alloc_one, 1024);

  double* data = static_cast<double*>(alloc_one.allocate(1024 * sizeof(double)));

  ASSERT_THROW(alloc_two.deallocate(data), umpire::runtime_error);

  ASSERT_NO_THROW(alloc_one.deallocate(data));
}

#if defined(UMPIRE_ENABLE_CUDA) || defined(UMPIRE_ENABLE_HIP) || defined(UMPIRE_ENABLE_SYCL)
TEST(Allocator, DeallocateDifferentUMDevice)
{
  auto& rm = umpire::ResourceManager::getInstance();
  auto alloc_um = rm.getAllocator("UM");
  auto alloc_dev = rm.getAllocator("DEVICE");

  double* data = static_cast<double*>(alloc_um.allocate(1024 * sizeof(double)));

  ASSERT_THROW(alloc_dev.deallocate(data), umpire::runtime_error);

  ASSERT_NO_THROW(alloc_um.deallocate(data));
}
#endif
