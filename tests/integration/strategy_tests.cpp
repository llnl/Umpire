//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#include <sstream>
#include <string>

#include "gtest/gtest.h"
#include "umpire/ResourceManager.hpp"
#include "umpire/config.hpp"
#include "umpire/util/AllocationHeader.hpp"
#include "umpire/strategy/AlignedAllocator.hpp"
#include "umpire/strategy/AllocationAdvisor.hpp"
#include "umpire/strategy/AllocationStrategy.hpp"
#include "umpire/strategy/DynamicPoolList.hpp"
#include "umpire/strategy/FixedPool.hpp"
#include "umpire/strategy/MixedPool.hpp"
#include "umpire/strategy/MonotonicAllocationStrategy.hpp"
#include "umpire/strategy/NamedAllocationStrategy.hpp"
#include "umpire/strategy/NamingShim.hpp"
#include "umpire/strategy/QuickPool.hpp"
#include "umpire/strategy/SizeLimiter.hpp"
#include "umpire/strategy/SlotPool.hpp"
#include "umpire/strategy/ThreadSafeAllocator.hpp"

#if defined(UMPIRE_ENABLE_NUMA)
#include "umpire/strategy/NumaPolicy.hpp"
#include "umpire/util/numa.hpp"
#endif

#include "umpire/Umpire.hpp"

#if defined(_OPENMP)
#include <omp.h>
#endif

#include <thread>

static int unique_strategy_id = 0;

#if defined(UMPIRE_ENABLE_INTROSPECTION_HEADER)
// Every tracked allocation is padded by the allocation header, which must be
// accounted for by tests with exact capacities or size limits
static constexpr std::size_t alloc_overhead{umpire::util::allocation_header_size};
#else
static constexpr std::size_t alloc_overhead{0};
#endif

const char* AllocationDevices[] = {"HOST"
#if defined(UMPIRE_ENABLE_DEVICE)
                                   ,
                                   "DEVICE"
#endif
#if defined(UMPIRE_ENABLE_UM)
                                   ,
                                   "UM"
#endif
#if defined(UMPIRE_ENABLE_PINNED)
                                   ,
                                   "PINNED"
#endif
#if defined(UMPIRE_ENABLE_FILE_RESOURCE)
                                   ,
                                   "FILE"
#endif
};

template <typename T>
class StrategyTest : public ::testing::Test {
 public:
  void SetUp() override
  {
    auto& rm = umpire::ResourceManager::getInstance();
    std::string name{"strategy_test_" + std::to_string(unique_strategy_id++)};

    m_allocator = new umpire::Allocator(rm.makeAllocator<T>(name, rm.getAllocator("HOST")));

    m_parent_name = "HOST";
  }

  void TearDown() override
  {
    delete m_allocator;
    m_allocator = nullptr;
  }

  umpire::Allocator* m_allocator;

  std::string m_parent_name;
  const std::size_t m_big = 64;
  const std::size_t m_nothing = 0;
};

template <>
void StrategyTest<umpire::strategy::FixedPool>::SetUp()
{
  auto& rm = umpire::ResourceManager::getInstance();
  std::string name{"strategy_test_" + std::to_string(unique_strategy_id++)};

  m_allocator = new umpire::Allocator(rm.makeAllocator<umpire::strategy::FixedPool>(
      name, rm.getAllocator("HOST"), m_big * sizeof(double) + alloc_overhead, 64));

  m_parent_name = "HOST";
}

#if defined(UMPIRE_ENABLE_CUDA) || defined(UMPIRE_ENABLE_HIP)
template <>
void StrategyTest<umpire::strategy::AllocationAdvisor>::SetUp()
{
  auto& rm = umpire::ResourceManager::getInstance();
  std::string name{"strategy_test_" + std::to_string(unique_strategy_id++)};

  m_allocator = new umpire::Allocator(
      rm.makeAllocator<umpire::strategy::AllocationAdvisor>(name, rm.getAllocator("UM"), "SET_READ_MOSTLY"));

  m_parent_name = "UM";
}
#endif

template <>
void StrategyTest<umpire::strategy::SizeLimiter>::SetUp()
{
  auto& rm = umpire::ResourceManager::getInstance();
  std::string name{"strategy_test_" + std::to_string(unique_strategy_id++)};

  m_allocator = new umpire::Allocator(rm.makeAllocator<umpire::strategy::SizeLimiter>(
      name, rm.getAllocator("HOST"), 4 * 1024 + 8 * alloc_overhead));

  m_parent_name = "HOST";
}

template <>
void StrategyTest<umpire::strategy::SlotPool>::SetUp()
{
  auto& rm = umpire::ResourceManager::getInstance();
  std::string name{"strategy_test_" + std::to_string(unique_strategy_id++)};

  m_allocator = new umpire::Allocator(
      rm.makeAllocator<umpire::strategy::SlotPool>(name, rm.getAllocator("HOST"), sizeof(double)));

  m_parent_name = "HOST";
}

template <>
void StrategyTest<umpire::strategy::MonotonicAllocationStrategy>::SetUp()
{
  auto& rm = umpire::ResourceManager::getInstance();
  std::string name{"strategy_test_" + std::to_string(unique_strategy_id++)};

  m_allocator = new umpire::Allocator(rm.makeAllocator<umpire::strategy::MonotonicAllocationStrategy>(
      name, rm.getAllocator("HOST"), 4 * 1024 + 8 * alloc_overhead));

  m_parent_name = "HOST";
}

using Strategies =
    ::testing::Types<umpire::strategy::AlignedAllocator,
#if defined(UMPIRE_ENABLE_CUDA) || defined(UMPIRE_ENABLE_HIP)
                     umpire::strategy::AllocationAdvisor,
#endif
                     umpire::strategy::DynamicPoolList, umpire::strategy::FixedPool, umpire::strategy::MixedPool,
                     umpire::strategy::MonotonicAllocationStrategy, umpire::strategy::NamedAllocationStrategy,
                     umpire::strategy::QuickPool, umpire::strategy::SizeLimiter, umpire::strategy::SlotPool,
                     umpire::strategy::ThreadSafeAllocator>;

TYPED_TEST_SUITE(StrategyTest, Strategies, );

TYPED_TEST(StrategyTest, AllocateDeallocateBig)
{
  double* data = static_cast<double*>(this->m_allocator->allocate(this->m_big * sizeof(double)));

  ASSERT_NE(nullptr, data);

  this->m_allocator->deallocate(data);
}

TYPED_TEST(StrategyTest, GetParentCheck)
{
  // Check to make sure the parent matches what is expected
  ASSERT_EQ(this->m_allocator->getParent()->getName(), this->m_parent_name);
}

TYPED_TEST(StrategyTest, MultipleAllocateDeallocate)
{
  const int number_of_allocations{8};
  std::vector<void*> allocations;

  for (int i{0}; i < number_of_allocations; ++i) {
    void* ptr = this->m_allocator->allocate(this->m_big * sizeof(double));
    ASSERT_NE(nullptr, ptr);
    allocations.push_back(ptr);
  }

  for (auto ptr : allocations) {
    this->m_allocator->deallocate(ptr);
  }
}

TYPED_TEST(StrategyTest, AllocateDeallocateNothing)
{
  double* data = static_cast<double*>(this->m_allocator->allocate(this->m_nothing * sizeof(double)));

  ASSERT_NE(nullptr, data);

  this->m_allocator->deallocate(data);
}

TYPED_TEST(StrategyTest, DeallocateNullptr)
{
  double* data = nullptr;

  ASSERT_NO_THROW(this->m_allocator->deallocate(data));

  SUCCEED();
}

TYPED_TEST(StrategyTest, GetSize)
{
  const std::size_t size = this->m_big * sizeof(double);

  double* data = static_cast<double*>(this->m_allocator->allocate(size));

  ASSERT_EQ(size, this->m_allocator->getSize(data));

  this->m_allocator->deallocate(data);
}

TYPED_TEST(StrategyTest, GetById)
{
  auto& rm = umpire::ResourceManager::getInstance();

  int id = this->m_allocator->getId();
  ASSERT_GE(id, 0);

  auto allocator_by_id = rm.getAllocator(id);

  ASSERT_EQ(this->m_allocator->getAllocationStrategy(), allocator_by_id.getAllocationStrategy());

  ASSERT_THROW(rm.getAllocator(-25), umpire::runtime_error);
}

TYPED_TEST(StrategyTest, get_allocator_records)
{
#if defined(UMPIRE_ENABLE_INTROSPECTION_HEADER)
  GTEST_SKIP() << "Allocation records cannot be enumerated with UMPIRE_ENABLE_INTROSPECTION_HEADER";
#endif

  double* data = static_cast<double*>(this->m_allocator->allocate(this->m_big * sizeof(double)));

  auto records = umpire::get_allocator_records(*(this->m_allocator));

  ASSERT_EQ(records.size(), 1);

  this->m_allocator->deallocate(data);
}

TYPED_TEST(StrategyTest, getCurrentSize)
{
  ASSERT_EQ(this->m_allocator->getCurrentSize(), 0);

  void* data = this->m_allocator->allocate(this->m_big * sizeof(double));

  // Strategies that track their own usage (e.g. MonotonicAllocationStrategy)
  // observe the header-padded size
  ASSERT_GE(this->m_allocator->getCurrentSize(), this->m_big * sizeof(double));
  ASSERT_LE(this->m_allocator->getCurrentSize(), this->m_big * sizeof(double) + alloc_overhead);

  this->m_allocator->deallocate(data);
}

TYPED_TEST(StrategyTest, getActualSize)
{
  void* data = this->m_allocator->allocate(this->m_big * sizeof(double));

  ASSERT_GE(this->m_allocator->getActualSize(), this->m_big * sizeof(double));

  this->m_allocator->deallocate(data);
}

template <typename T>
class ReleaseTest : public ::testing::Test {
 public:
  void SetUp() override
  {
    auto& rm = umpire::ResourceManager::getInstance();
    std::string name{"release_test_" + std::to_string(unique_strategy_id++)};
    std::string limiter_name{"limiter_" + std::to_string(unique_strategy_id++)};

    m_limiter_allocator = new umpire::Allocator(rm.makeAllocator<umpire::strategy::SizeLimiter>(
        limiter_name, rm.getAllocator("HOST"), (max_alloc_size + alloc_overhead) * num_allocs + padding));

    m_allocator = new umpire::Allocator(
        rm.makeAllocator<T>(name, rm.getAllocator(limiter_name), (max_alloc_size + alloc_overhead) * num_allocs));
  }

  void TearDown() override
  {
    delete m_allocator;
    m_allocator = nullptr;

    delete m_limiter_allocator;
    m_limiter_allocator = nullptr;
  }

  ////////////////////////////////////////////////////////////////////////
  // max_alloc_size: Biggest allocation size used in test.
  // num_allocs: The number of total allocations
  // padding: Some strategies have built-in alignment handling which will
  //  interfere will this test. Padding helps make sure we account for that.
  ////////////////////////////////////////////////////////////////////////
  const int max_alloc_size = 1024;
  static const int num_allocs = 8;
  const int padding = 64;
  void* test[num_allocs] = {0};

  umpire::Allocator* m_allocator;
  umpire::Allocator* m_limiter_allocator;
};

template <>
void ReleaseTest<umpire::strategy::FixedPool>::SetUp()
{
  auto& rm = umpire::ResourceManager::getInstance();
  std::string name{"release_test_" + std::to_string(unique_strategy_id++)};
  std::string limiter_name{"limiter_" + std::to_string(unique_strategy_id++)};

  m_limiter_allocator = new umpire::Allocator(rm.makeAllocator<umpire::strategy::SizeLimiter>(
      limiter_name, rm.getAllocator("HOST"), (max_alloc_size + alloc_overhead) * num_allocs));

  m_allocator = new umpire::Allocator(rm.makeAllocator<umpire::strategy::FixedPool>(
      name, rm.getAllocator(limiter_name), max_alloc_size + alloc_overhead, 1));
}

using ReleaseStrategies =
    ::testing::Types<umpire::strategy::DynamicPoolList, umpire::strategy::FixedPool, umpire::strategy::QuickPool>;

TYPED_TEST_SUITE(ReleaseTest, ReleaseStrategies, );

TYPED_TEST(ReleaseTest, ReleaseCheck)
{
  for (int i = 0; i < this->num_allocs; i++) {
    this->test[i] = this->m_allocator->allocate(this->max_alloc_size);
  }

  this->m_allocator->release(); // this should have no effect
  ASSERT_THROW(this->test[0] = this->m_limiter_allocator->allocate(this->max_alloc_size), umpire::runtime_error);

  for (int i = 0; i < this->num_allocs; i++) {
    this->m_allocator->deallocate(this->test[i]);
  }

  ASSERT_THROW(this->test[0] = this->m_limiter_allocator->allocate(this->max_alloc_size), umpire::runtime_error);
  this->m_allocator->release();

  ASSERT_NO_THROW(this->test[0] = this->m_limiter_allocator->allocate(this->max_alloc_size));
  ASSERT_NO_THROW(this->m_limiter_allocator->deallocate(this->test[0]));
}

TEST(MonotonicStrategy, Host)
{
  auto& rm = umpire::ResourceManager::getInstance();

  auto allocator = rm.makeAllocator<umpire::strategy::MonotonicAllocationStrategy>("host_monotonic_pool",
                                                                                   rm.getAllocator("HOST"), 65536);

  void* alloc = allocator.allocate(100);
  void* alloc2 = allocator.allocate(100);

  ASSERT_EQ(static_cast<char*>(alloc2) - static_cast<char*>(alloc), 100 + alloc_overhead);
  ASSERT_GE(allocator.getCurrentSize(), 100);
  ASSERT_EQ(allocator.getSize(alloc), 100);
  ASSERT_GE(allocator.getHighWatermark(), 100);
  ASSERT_EQ(allocator.getName(), "host_monotonic_pool");

  allocator.deallocate(alloc2);
  allocator.deallocate(alloc);
}

#if defined(UMPIRE_ENABLE_DEVICE)
TEST(MonotonicStrategy, Device)
{
  auto& rm = umpire::ResourceManager::getInstance();

  auto allocator = rm.makeAllocator<umpire::strategy::MonotonicAllocationStrategy>("device_monotonic_pool",
                                                                                   rm.getAllocator("DEVICE"), 65536);

  void* alloc = allocator.allocate(100);

  ASSERT_GE(allocator.getCurrentSize(), 100);
  ASSERT_EQ(allocator.getSize(alloc), 100);
  ASSERT_GE(allocator.getHighWatermark(), 100);
  ASSERT_EQ(allocator.getName(), "device_monotonic_pool");

  allocator.deallocate(alloc);
}
#endif // defined(UMPIRE_ENABLE_DEVICE)

#if defined(UMPIRE_ENABLE_UM)
TEST(MonotonicStrategy, UM)
{
  auto& rm = umpire::ResourceManager::getInstance();

  auto allocator = rm.makeAllocator<umpire::strategy::MonotonicAllocationStrategy>("um_monotonic_pool",
                                                                                   rm.getAllocator("UM"), 65536);

  void* alloc = allocator.allocate(100);

  ASSERT_GE(allocator.getCurrentSize(), 100);
  ASSERT_EQ(allocator.getSize(alloc), 100);
  ASSERT_GE(allocator.getHighWatermark(), 100);
  ASSERT_EQ(allocator.getName(), "um_monotonic_pool");

  allocator.deallocate(alloc);
}
#endif // defined(UMPIRE_ENABLE_UM)

#if defined(UMPIRE_ENABLE_CUDA) || defined(UMPIRE_ENABLE_HIP)
TEST(AllocationAdvisor, Create)
{
  auto& rm = umpire::ResourceManager::getInstance();

  ASSERT_NO_THROW(auto read_only_alloc = rm.makeAllocator<umpire::strategy::AllocationAdvisor>(
                      "read_only_um", rm.getAllocator("UM"), "SET_READ_MOSTLY");
                  UMPIRE_USE_VAR(read_only_alloc));

  ASSERT_ANY_THROW(auto failed_alloc = rm.makeAllocator<umpire::strategy::AllocationAdvisor>(
                       "read_only_um_nonsense_operator", rm.getAllocator("UM"), "FOOBAR");
                   UMPIRE_USE_VAR(failed_alloc));
}

TEST(AllocationAdvisor, CreateWithId)
{
  auto& rm = umpire::ResourceManager::getInstance();

  const int device_id = 2;

  ASSERT_NO_THROW(auto read_only_alloc = rm.makeAllocator<umpire::strategy::AllocationAdvisor>(
                      "read_only_um_device_id", rm.getAllocator("UM"), "SET_READ_MOSTLY", device_id);
                  UMPIRE_USE_VAR(read_only_alloc));

  ASSERT_ANY_THROW(auto failed_alloc = rm.makeAllocator<umpire::strategy::AllocationAdvisor>(
                       "read_only_um_nonsense_operator_device_id", rm.getAllocator("UM"), "FOOBAR", device_id);
                   UMPIRE_USE_VAR(failed_alloc));
}

TEST(AllocationAdvisor, Host)
{
  auto& rm = umpire::ResourceManager::getInstance();
  auto um_allocator = rm.getAllocator("UM");
  auto host_allocator = rm.getAllocator("HOST");

  auto read_only_alloc = rm.makeAllocator<umpire::strategy::AllocationAdvisor>(
      "preferred_location_host", um_allocator, "SET_PREFERRED_LOCATION", host_allocator);

  ASSERT_NO_THROW({
    double* data = static_cast<double*>(read_only_alloc.allocate(1024 * sizeof(double)));
    read_only_alloc.deallocate(data);
  });
}
#endif // defined(UMPIRE_ENABLE_CUDA) || defined(UMPIRE_ENABLE_HIP)

TEST(FixedPool, Host)
{
  auto& rm = umpire::ResourceManager::getInstance();

  const int data_size = 100 * sizeof(int);

  auto allocator =
      rm.makeAllocator<umpire::strategy::FixedPool>("host_fixed_pool", rm.getAllocator("HOST"), data_size, 64);

  void* alloc = allocator.allocate(data_size);

  ASSERT_EQ(allocator.getCurrentSize(), data_size);
  ASSERT_GE(allocator.getActualSize(), data_size * 64);
  ASSERT_EQ(allocator.getSize(alloc), data_size);
  ASSERT_GE(allocator.getHighWatermark(), data_size);
  ASSERT_EQ(allocator.getName(), "host_fixed_pool");

  allocator.deallocate(alloc);
}

TEST(MixedPool, Host)
{
  auto& rm = umpire::ResourceManager::getInstance();

  auto allocator = rm.makeAllocator<umpire::strategy::MixedPool>("host_mixed_pool", rm.getAllocator("HOST"));

  const std::size_t max_power = 9;
  void* alloc[max_power];
  std::size_t size = 4, total_size = 0;
  for (std::size_t i = 0; i < max_power; ++i) {
    alloc[i] = allocator.allocate(size);
    total_size += size;
    size *= 4;
  }

  ASSERT_EQ(allocator.getCurrentSize(), total_size);
  ASSERT_GT(allocator.getActualSize(), total_size);
  ASSERT_EQ(allocator.getSize(alloc[max_power - 1]), size / 4);
  ASSERT_GE(allocator.getHighWatermark(), total_size);
  ASSERT_EQ(allocator.getName(), "host_mixed_pool");

  for (std::size_t i = 0; i < max_power; ++i)
    allocator.deallocate(alloc[i]);
}

TEST(ThreadSafeAllocator, HostStdThread)
{
  auto& rm = umpire::ResourceManager::getInstance();

  auto allocator = rm.makeAllocator<umpire::strategy::ThreadSafeAllocator>("thread_safe_allocator_host_std",
                                                                           rm.getAllocator("HOST"));

  constexpr int N = 16;
  std::vector<void*> thread_allocs{N};
  std::vector<std::thread> threads;

  for (std::size_t i = 0; i < N; i++) {
    threads.push_back(std::thread([=, &allocator, &thread_allocs] {
      for (int j = 0; j < N; ++j) {
        thread_allocs[i] = allocator.allocate(1024);
        ASSERT_NE(thread_allocs[i], nullptr);
        allocator.deallocate(thread_allocs[i]);
      }
      thread_allocs[i] = allocator.allocate(1024);
      ASSERT_NE(thread_allocs[i], nullptr);
    }));
  }

  for (auto& t : threads) {
    t.join();
  }

  for (auto alloc : thread_allocs) {
    ASSERT_NE(alloc, nullptr);
  }

  ASSERT_NO_THROW({
    for (auto alloc : thread_allocs) {
      allocator.deallocate(alloc);
    }
  });
}

#if defined(_OPENMP)
TEST(ThreadSafeAllocator, HostOpenMP)
{
  auto& rm = umpire::ResourceManager::getInstance();

  auto allocator = rm.makeAllocator<umpire::strategy::ThreadSafeAllocator>("thread_safe_allocator_host_omp",
                                                                           rm.getAllocator("HOST"));

#pragma omp parallel
  {
    const std::size_t size = 1024 * omp_get_thread_num();

    double* thread_data = static_cast<double*>(allocator.allocate(size * sizeof(double)));

    allocator.deallocate(thread_data);
  }

  SUCCEED();
}
#endif

#if defined(UMPIRE_ENABLE_DEVICE)
TEST(ThreadSafeAllocator, DeviceStdThread)
{
  auto& rm = umpire::ResourceManager::getInstance();

  auto allocator = rm.makeAllocator<umpire::strategy::ThreadSafeAllocator>("thread_safe_allocator_device_std",
                                                                           rm.getAllocator("DEVICE"));

  constexpr int N = 16;
  std::vector<void*> thread_allocs{N};
  std::vector<std::thread> threads;

  for (std::size_t i = 0; i < N; ++i) {
    threads.push_back(std::thread([=, &allocator, &thread_allocs] {
      for (int j = 0; j < N; ++j) {
        thread_allocs[i] = allocator.allocate(1024);
        ASSERT_NE(thread_allocs[i], nullptr);
        allocator.deallocate(thread_allocs[i]);
        thread_allocs[i] = allocator.allocate(1024);
        ASSERT_NE(thread_allocs[i], nullptr);
      }
    }));
  }

  for (auto& t : threads) {
    t.join();
  }

  for (auto alloc : thread_allocs) {
    ASSERT_NE(alloc, nullptr);
  }

  ASSERT_NO_THROW({
    for (auto alloc : thread_allocs) {
      allocator.deallocate(alloc);
    }
  });
}

#if defined(_OPENMP)
TEST(ThreadSafeAllocator, DeviceOpenMP)
{
  auto& rm = umpire::ResourceManager::getInstance();

  auto allocator = rm.makeAllocator<umpire::strategy::ThreadSafeAllocator>("thread_safe_allocator_device_omp",
                                                                           rm.getAllocator("DEVICE"));

#pragma omp parallel
  {
    const std::size_t size = 1024 * omp_get_thread_num();

    double* thread_data = static_cast<double*>(allocator.allocate(size * sizeof(double)));

    allocator.deallocate(thread_data);
  }

  SUCCEED();
}
#endif
#endif // defined(UMPIRE_ENABLE_DEVICE)

TEST(SizeLimiter, Host)
{
  auto& rm = umpire::ResourceManager::getInstance();

  auto alloc = rm.makeAllocator<umpire::strategy::SizeLimiter>("size_limited_alloc", rm.getAllocator("HOST"),
                                                               64 + alloc_overhead);

  void* data = nullptr;

  EXPECT_NO_THROW(data = alloc.allocate(64));

  EXPECT_THROW(
      {
        void* tmp_data = alloc.allocate(1024);
        UMPIRE_USE_VAR(tmp_data);
      },
      umpire::out_of_memory_error);

  EXPECT_NO_THROW(alloc.deallocate(data));
}

#if defined(UMPIRE_ENABLE_NUMA)
TEST(NumaPolicyTest, EdgeCases)
{
  auto& rm = umpire::ResourceManager::getInstance();

  EXPECT_THROW(rm.makeAllocator<umpire::strategy::NumaPolicy>("numa_alloc", rm.getAllocator("HOST"), -1),
               umpire::runtime_error);

#if defined(UMPIRE_ENABLE_CUDA) || defined(UMPIRE_ENABLE_HIP)
  const int numa_node = umpire::numa::preferred_node();

  // Only works with HOST allocators
  EXPECT_THROW(rm.makeAllocator<umpire::strategy::NumaPolicy>("numa_alloc", rm.getAllocator("DEVICE"), numa_node),
               umpire::runtime_error);
#endif
}

TEST(NumaPolicyTest, Location)
{
  auto& rm = umpire::ResourceManager::getInstance();

  auto nodes = umpire::numa::get_host_nodes();
  for (auto n : nodes) {
    std::stringstream ss;
    ss << "numa_alloc_" << n;

    auto alloc = rm.makeAllocator<umpire::strategy::NumaPolicy>(ss.str(), rm.getAllocator("HOST"), n);

    void* ptr = alloc.allocate(10 * umpire::get_page_size());

    rm.memset(ptr, 0);
    ASSERT_EQ(umpire::numa::get_location(ptr), n);

    alloc.deallocate(ptr);
  }
}

#endif // defined(UMPIRE_ENABLE_NUMA)

static inline void test_alignment(uintptr_t p, unsigned int align)
{
  ASSERT_EQ(0, p % align);
  p &= align;
  ASSERT_TRUE(p == 0 || p == align);
}

TEST(AlignedAllocator, AllocateAlign256)
{
#if defined(UMPIRE_ENABLE_INTROSPECTION_HEADER)
  GTEST_SKIP() << "AlignedAllocator alignments beyond the allocation header alignment are not preserved "
                  "with UMPIRE_ENABLE_INTROSPECTION_HEADER";
#endif

  unsigned int align = 256;
  auto& rm = umpire::ResourceManager::getInstance();
  auto alloc =
      rm.makeAllocator<umpire::strategy::AlignedAllocator>("aligned_allocator_256", rm.getAllocator("HOST"), align);

  void* d1{alloc.allocate(1)};
  void* d2{alloc.allocate(257)};
  void* d3{alloc.allocate(783)};

  test_alignment(reinterpret_cast<uintptr_t>(d1), align);
  test_alignment(reinterpret_cast<uintptr_t>(d2), align);
  test_alignment(reinterpret_cast<uintptr_t>(d3), align);

  alloc.deallocate(d1);
  alloc.deallocate(d2);
  alloc.deallocate(d3);
}

TEST(AlignedAllocator, AllocateAlign64)
{
#if defined(UMPIRE_ENABLE_INTROSPECTION_HEADER)
  GTEST_SKIP() << "AlignedAllocator alignments beyond the allocation header alignment are not preserved "
                  "with UMPIRE_ENABLE_INTROSPECTION_HEADER";
#endif

  unsigned int align = 64;
  auto& rm = umpire::ResourceManager::getInstance();
  auto alloc =
      rm.makeAllocator<umpire::strategy::AlignedAllocator>("aligned_allocator_64", rm.getAllocator("HOST"), align);

  void* d1{alloc.allocate(1)};
  void* d2{alloc.allocate(17)};
  void* d3{alloc.allocate(128)};

  test_alignment(reinterpret_cast<uintptr_t>(d1), align);
  test_alignment(reinterpret_cast<uintptr_t>(d2), align);
  test_alignment(reinterpret_cast<uintptr_t>(d3), align);

  alloc.deallocate(d1);
  alloc.deallocate(d2);
  alloc.deallocate(d3);
}

TEST(AlignedAllocator, BadAlignment)
{
  auto& rm = umpire::ResourceManager::getInstance();

  EXPECT_THROW(
      {
        auto alloc =
            rm.makeAllocator<umpire::strategy::AlignedAllocator>("aligned_allocator_6", rm.getAllocator("HOST"), 6);
        UMPIRE_USE_VAR(alloc);
      },
      umpire::runtime_error);

  EXPECT_THROW(
      {
        auto alloc =
            rm.makeAllocator<umpire::strategy::AlignedAllocator>("aligned_allocator_11", rm.getAllocator("HOST"), 11);
        UMPIRE_USE_VAR(alloc);
      },
      umpire::runtime_error);

  EXPECT_THROW(
      {
        auto alloc =
            rm.makeAllocator<umpire::strategy::AlignedAllocator>("aligned_allocator_0", rm.getAllocator("HOST"), 0);
        UMPIRE_USE_VAR(alloc);
      },
      umpire::runtime_error);
}
#if defined(UMPIRE_ENABLE_IPC_SHARED_MEMORY)
TEST(NamingShimTests, TestAllocateDeallocate)
{
  auto& rm = umpire::ResourceManager::getInstance();

  auto traits{umpire::get_default_resource_traits("SHARED::POSIX")};
  traits.size = 1 * 1024 * 1024;
  traits.scope = umpire::MemoryResourceTraits::shared_scope::node;

  // NOTE: The name of the allocator MUST have "SHARED::POSIX:: prefix when both IPC and MPI3 enabled.
  auto node_allocator{rm.makeResource("SHARED::POSIX::shim_allocator", traits)};

  auto shim{rm.makeAllocator<umpire::strategy::NamingShim>("shim", node_allocator)};
  {
    void* ptr = shim.allocate(1024);
    EXPECT_NE(ptr, nullptr);
    shim.deallocate(ptr);
  }
}

TEST(NamedAllocatorTest, ForwardName)
{
  auto& rm = umpire::ResourceManager::getInstance();

  auto traits{umpire::get_default_resource_traits("SHARED::POSIX")};
  traits.size = 1 * 1024 * 1024;
  traits.scope = umpire::MemoryResourceTraits::shared_scope::node;

  // NOTE: The name of the allocator MUST have "SHARED::POSIX:: prefix when both IPC and MPI3 enabled.
  auto node_allocator{rm.makeResource("SHARED::POSIX::allocator_for_named_test", traits)};

  auto allocator{rm.makeAllocator<umpire::strategy::NamedAllocationStrategy>("shared named alloc", node_allocator)};
  {
    void* ptr = allocator.allocate("test", 1024);
    EXPECT_NE(ptr, nullptr);
    allocator.deallocate(ptr);
  }
}
#endif
