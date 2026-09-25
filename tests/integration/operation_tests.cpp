//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#include "test_helpers.hpp"
#include "umpire/Allocator.hpp"
#include "umpire/ResourceManager.hpp"
#include "umpire/config.hpp"
#include "umpire/op/MemoryOperationRegistry.hpp"
#include "umpire/strategy/AlignedAllocator.hpp"
#include "umpire/strategy/AllocationAdvisor.hpp"
#include "umpire/strategy/AllocationStrategy.hpp"
#include "umpire/strategy/DynamicPoolList.hpp"
#include "umpire/strategy/FixedPool.hpp"
#include "umpire/strategy/MixedPool.hpp"
#include "umpire/strategy/MonotonicAllocationStrategy.hpp"
#include "umpire/strategy/NamedAllocationStrategy.hpp"
#include "umpire/strategy/QuickPool.hpp"
#include "umpire/strategy/SizeLimiter.hpp"
#include "umpire/strategy/SlotPool.hpp"
#include "umpire/strategy/ThreadSafeAllocator.hpp"
#include "umpire/util/error.hpp"

#if defined(UMPIRE_ENABLE_CUDA)
#include <cuda_runtime_api.h>
#endif

#include "camp/camp.hpp"
#include "gtest/gtest.h"

#if defined(UMPIRE_ENABLE_CUDA)
using resource_type = camp::resources::Cuda;
#elif defined(UMPIRE_ENABLE_HIP)
using resource_type = camp::resources::Hip;
#else
using resource_type = camp::resources::Host;
#endif

//
// Operations on a pointer offset into an allocation require an interior
// pointer lookup, which allocation headers cannot provide.
//
#if defined(UMPIRE_ENABLE_INTROSPECTION_HEADER)
#define SKIP_IF_NO_OFFSET_LOOKUP() \
  GTEST_SKIP() << "Interior pointer lookup is not supported with UMPIRE_ENABLE_INTROSPECTION_HEADER"
#else
#define SKIP_IF_NO_OFFSET_LOOKUP() ((void)0)
#endif

static int s_counter{0};

struct NullStrategy {
};

// Note: memset & reallocate tests currently require the complete source list
// and only support a single destination.
using HostAccessibleResources = camp::list<host_resource_tag
#if defined(UMPIRE_ENABLE_UM)
                                           ,
                                           um_resource_tag
#endif
#if defined(UMPIRE_ENABLE_PINNED)
                                           ,
                                           pinned_resource_tag
#endif
                                           >;

using AllResources = camp::list<host_resource_tag
#if defined(UMPIRE_ENABLE_DEVICE)
                                ,
                                device_resource_tag
#endif
#if defined(UMPIRE_ENABLE_UM)
                                ,
                                um_resource_tag
#endif
#if defined(UMPIRE_ENABLE_PINNED)
                                ,
                                pinned_resource_tag
#endif
                                >;

using Strategies =
    camp::list<NullStrategy
#if !(defined(UMPIRE_ENABLE_HIP) || defined(UMPIRE_ENABLE_CUDA) || defined(UMPIRE_ENABLE_SYCL) || \
      defined(UMPIRE_ENABLE_OPENMP_TARGET) || defined(__PGI))
               ,
               umpire::strategy::AlignedAllocator, umpire::strategy::DynamicPoolList, umpire::strategy::QuickPool,
               umpire::strategy::MixedPool, umpire::strategy::NamedAllocationStrategy, umpire::strategy::SizeLimiter,
               umpire::strategy::SlotPool, umpire::strategy::ThreadSafeAllocator
#endif
               >;

using TestTypes = camp::cartesian_product<HostAccessibleResources, AllResources, Strategies, Strategies>;
using SourceTypes =
    camp::cartesian_product<AllResources, camp::list<host_resource_tag>, Strategies, camp::list<NullStrategy>>;

using AllTestTypes = Test<TestTypes>::Types;
using SourceTestTypes = Test<SourceTypes>::Types;

template <typename Strategy>
struct make_allocator_helper {
  static umpire::Allocator make(const std::string& name, const std::string& resource_name)
  {
    auto& rm = umpire::ResourceManager::getInstance();
    return rm.makeAllocator<Strategy>(name, rm.getAllocator(resource_name));
  }
};

template <>
struct make_allocator_helper<NullStrategy> {
  static umpire::Allocator make(const std::string&, const std::string& resource_name)
  {
    auto& rm = umpire::ResourceManager::getInstance();
    return rm.getAllocator(resource_name);
  }
};

template <>
struct make_allocator_helper<umpire::strategy::SlotPool> {
  static umpire::Allocator make(const std::string& name, const std::string& resource_name)
  {
    auto& rm = umpire::ResourceManager::getInstance();
    return rm.makeAllocator<umpire::strategy::SlotPool>(name, rm.getAllocator(resource_name), 256);
  }
};

template <>
struct make_allocator_helper<umpire::strategy::SizeLimiter> {
  static umpire::Allocator make(const std::string& name, const std::string& resource_name)
  {
    auto& rm = umpire::ResourceManager::getInstance();
    return rm.makeAllocator<umpire::strategy::SizeLimiter>(name, rm.getAllocator(resource_name), 1024 * 1024 * 1024);
  }
};

template <>
struct make_allocator_helper<umpire::strategy::AllocationAdvisor> {
  static umpire::Allocator make(const std::string& name, const std::string& resource_name)
  {
    auto& rm = umpire::ResourceManager::getInstance();
    return rm.makeAllocator<umpire::strategy::AllocationAdvisor>(name, rm.getAllocator(resource_name),
                                                                 "SET_PREFERRED_LOCATION");
  }
};

template <typename LocationStrategyTuple>
class OperationTest : public ::testing::Test {
 public:
  using SourceResource = typename camp::at<LocationStrategyTuple, camp::num<0>>::type;
  using DestResource = typename camp::at<LocationStrategyTuple, camp::num<1>>::type;

  using SourceStrategy = typename camp::at<LocationStrategyTuple, camp::num<2>>::type;
  using DestStrategy = typename camp::at<LocationStrategyTuple, camp::num<3>>::type;

  void SetUp() override
  {
    const std::string source_resource_name{tag_to_string<SourceResource>::value};
    const std::string dest_resource_name{tag_to_string<DestResource>::value};

    std::string source_name =
        std::string{"operation_test_"} + source_resource_name + std::string{"_"} + std::to_string(s_counter++);
    std::string dest_name =
        std::string{"operation_test_"} + dest_resource_name + std::string{"_"} + std::to_string(s_counter++);

    auto& rm = umpire::ResourceManager::getInstance();

    source_allocator =
        new umpire::Allocator(make_allocator_helper<SourceStrategy>::make(source_name, source_resource_name));

    dest_allocator = new umpire::Allocator(make_allocator_helper<DestStrategy>::make(dest_name, dest_resource_name));

    source_array = static_cast<float*>(source_allocator->allocate(m_size * sizeof(float)));
    dest_array = static_cast<float*>(dest_allocator->allocate(m_size * sizeof(float)));

    check_array = static_cast<float*>(rm.getAllocator("HOST").allocate(m_size * sizeof(float)));
  }

  void TearDown() override
  {
    auto& rm = umpire::ResourceManager::getInstance();

    if (source_array)
      source_allocator->deallocate(source_array);

    if (dest_array)
      dest_allocator->deallocate(dest_array);

    if (check_array)
      rm.getAllocator("HOST").deallocate(check_array);

    delete source_allocator;
    delete dest_allocator;
  }

  float* source_array;
  float* dest_array;
  float* check_array;

  const std::size_t m_size = 1024;

  umpire::Allocator* source_allocator;
  umpire::Allocator* dest_allocator;
};

template <typename T>
class ZeroCopyTest : public OperationTest<T> {
};

TYPED_TEST_SUITE(ZeroCopyTest, AllTestTypes, );

TYPED_TEST(ZeroCopyTest, Zero)
{
  auto& rm = umpire::ResourceManager::getInstance();

  void* src = this->source_allocator->allocate(0);
  void* dst = this->dest_allocator->allocate(0);

  rm.copy(dst, src);

  this->source_allocator->deallocate(src);
  this->dest_allocator->deallocate(dst);
}

template <typename T>
class CopyTest : public OperationTest<T> {
};

TYPED_TEST_SUITE(CopyTest, AllTestTypes, );

TYPED_TEST(CopyTest, Copy)
{
  auto& rm = umpire::ResourceManager::getInstance();

  for (std::size_t i = 0; i < this->m_size; i++) {
    this->source_array[i] = static_cast<float>(i);
  }

  rm.copy(this->dest_array, this->source_array);

  rm.copy(this->check_array, this->dest_array);

  for (std::size_t i = 0; i < this->m_size; i++) {
    ASSERT_FLOAT_EQ(this->source_array[i], this->check_array[i]);
  }
}

TYPED_TEST(CopyTest, Single)
{
  SKIP_IF_NO_OFFSET_LOOKUP();

  auto& rm = umpire::ResourceManager::getInstance();

  this->check_array[10] = 3.14f;

  rm.copy(&(this->dest_array)[11], &(this->check_array)[10], sizeof(float));
  rm.copy(&(this->check_array)[0], &(this->dest_array)[11], sizeof(float));

  ASSERT_EQ(this->check_array[0], this->check_array[10]);
}

TYPED_TEST(CopyTest, Offset)
{
  SKIP_IF_NO_OFFSET_LOOKUP();

  auto& rm = umpire::ResourceManager::getInstance();

  for (std::size_t i = 0; i < this->m_size; ++i) {
    this->source_array[i] = static_cast<float>(i);
  }

  rm.copy(this->dest_array, this->source_array);

  rm.copy(this->check_array, this->dest_array);
  rm.copy(this->check_array, this->dest_array + this->m_size / 2);

  for (std::size_t i = 0; i < this->m_size / 2; ++i) {
    ASSERT_EQ(i + this->m_size / 2, this->check_array[i]);
  }

  for (std::size_t i = this->m_size / 2; i < this->m_size; ++i) {
    ASSERT_EQ(i, this->check_array[i]);
  }
}

TYPED_TEST(CopyTest, InvalidSize)
{
  auto& rm = umpire::ResourceManager::getInstance();

  ASSERT_THROW(rm.copy(this->dest_array, this->source_array, (this->m_size + 100) * sizeof(float)),
               umpire::runtime_error);

  float* small_dest_array = static_cast<float*>(this->dest_allocator->allocate(10 * sizeof(float)));

  ASSERT_THROW(rm.copy(small_dest_array, this->source_array), umpire::runtime_error);

  this->dest_allocator->deallocate(small_dest_array);
}

TYPED_TEST(CopyTest, Async)
{
  auto& rm = umpire::ResourceManager::getInstance();
  auto resource = camp::resources::Resource{resource_type{}};

  for (std::size_t i = 0; i < this->m_size; i++) {
    this->source_array[i] = static_cast<float>(i);
  }

  camp::resources::Event event = rm.copy(this->dest_array, this->source_array, resource);
  event = rm.copy(this->check_array, this->dest_array, resource);

  for (std::size_t i = 0; i < this->m_size; i++) {
    ASSERT_FLOAT_EQ(this->source_array[i], this->check_array[i]);
  }
}

template <typename T>
class MemsetTest : public OperationTest<T> {
};

TYPED_TEST_SUITE(MemsetTest, SourceTestTypes, );

TYPED_TEST(MemsetTest, Memset)
{
  auto& rm = umpire::ResourceManager::getInstance();

  rm.memset(this->source_array, 0);

  rm.copy(this->check_array, this->source_array);

  for (std::size_t i = 0; i < this->m_size; i++) {
    ASSERT_EQ(0, this->check_array[i]);
  }
}

TYPED_TEST(MemsetTest, Offset)
{
  SKIP_IF_NO_OFFSET_LOOKUP();

  auto& rm = umpire::ResourceManager::getInstance();

  rm.memset(this->source_array, 1);
  rm.memset(this->source_array + this->m_size / 2, 2);

  rm.copy(this->check_array, this->source_array);

  char* check_chars = reinterpret_cast<char*>(this->check_array);

  for (std::size_t i = 0; i < this->m_size / 2 * sizeof(float); ++i) {
    ASSERT_EQ(1, check_chars[i]);
  }

  for (std::size_t i = this->m_size / 2 * sizeof(float); i < this->m_size * sizeof(float); ++i) {
    ASSERT_EQ(2, check_chars[i]);
  }
}

TYPED_TEST(MemsetTest, InvalidSize)
{
  auto& rm = umpire::ResourceManager::getInstance();

  ASSERT_THROW(rm.memset(this->source_array, 0, (this->m_size + 100) * sizeof(float)), umpire::runtime_error);
}

TYPED_TEST(MemsetTest, InvalidPointer)
{
  auto& rm = umpire::ResourceManager::getInstance();

  ASSERT_THROW(rm.memset((void*)0x1, 0), umpire::runtime_error);
}

template <typename T>
class ReallocateTest : public OperationTest<T> {
};

TYPED_TEST_SUITE(ReallocateTest, SourceTestTypes, );

// This value is such that the 64Kb limit on device constant memory is not hit
// in check_alloc_realloc_free when reallocating to 3 * SIZE.
constexpr int SIZE = 5345;

TYPED_TEST(ReallocateTest, ReallocateSweep)
{
  umpire::ResourceManager& rm = umpire::ResourceManager::getInstance();
  const bool hostAccessible = this->source_allocator->getId() == rm.getAllocator("HOST").getId();

  for (int size = 0; size <= SIZE; size = size * 2 + 1) {
    int buffer_size = size;
    int* buffer = nullptr;

    ASSERT_NO_THROW({ buffer = static_cast<int*>(this->source_allocator->allocate(buffer_size * sizeof(*buffer))); });

    ASSERT_EQ(this->source_allocator->getId(), rm.getAllocator(buffer).getId());

    if (hostAccessible) {
      // Populate the buffer.
      for (int i = 0; i < buffer_size; ++i) {
        buffer[i] = i;
      }

      // Check the values.
      for (int i = 0; i < buffer_size; ++i) {
        ASSERT_EQ(buffer[i], i);
      }
    }

    // Reallocate to a larger size.
    buffer_size *= 3;
    ASSERT_NO_THROW({ buffer = static_cast<int*>(rm.reallocate(buffer, buffer_size * sizeof(*buffer))); });
    if (buffer_size > 0) {
      ASSERT_EQ(this->source_allocator->getId(), rm.getAllocator(buffer).getId());
    }

    if (hostAccessible) {
      // Populate the new values.
      for (int i = size; i < buffer_size; ++i) {
        buffer[i] = i;
      }

      // Check all the values.
      for (int i = 0; i < buffer_size; ++i) {
        EXPECT_EQ(buffer[i], i);
      }
    }

    // Reallocate to a smaller size.
    buffer_size /= 5;
    ASSERT_NO_THROW({ buffer = static_cast<int*>(rm.reallocate(buffer, buffer_size * sizeof(*buffer))); });
    if (buffer_size > 0) {
      ASSERT_EQ(this->source_allocator->getId(), rm.getAllocator(buffer).getId());
    }

    if (hostAccessible) {
      // Check all the values.
      for (int i = 0; i < buffer_size; ++i) {
        EXPECT_EQ(buffer[i], i);
      }
    }

    // Free
    ASSERT_NO_THROW({ this->source_allocator->deallocate(buffer); });
    // EXPECT_TRUE( buffer == nullptr );
  }
}

TYPED_TEST(ReallocateTest, Reallocate)
{
  umpire::ResourceManager& rm = umpire::ResourceManager::getInstance();

  const std::size_t reallocated_size = (this->m_size / 2);

  rm.memset(this->source_array, 1);

  this->source_array = static_cast<float*>(rm.reallocate(this->source_array, reallocated_size * sizeof(float)));

  ASSERT_EQ(this->source_allocator->getSize(this->source_array), reallocated_size * sizeof(float));

  rm.copy(this->check_array, this->source_array, reallocated_size * sizeof(float));

  auto checker = reinterpret_cast<char*>(this->check_array);
  for (std::size_t i = 0; i < reallocated_size * (sizeof(float) / sizeof(char)); i++) {
    ASSERT_FLOAT_EQ(checker[i], 1);
  }
}

TYPED_TEST(ReallocateTest, ReallocateLarger)
{
  SKIP_IF_NO_OFFSET_LOOKUP();

  umpire::ResourceManager& rm = umpire::ResourceManager::getInstance();

  const std::size_t reallocated_size = (this->m_size + 50);

  rm.memset(this->source_array, 1);

  this->source_array = static_cast<float*>(rm.reallocate(this->source_array, reallocated_size * sizeof(float)));

  ASSERT_EQ(this->source_allocator->getSize(this->source_array), reallocated_size * sizeof(float));

  rm.memset(this->source_array + this->m_size, 2);

  this->check_array = static_cast<float*>(rm.reallocate(this->check_array, reallocated_size * sizeof(float)));

  rm.copy(this->check_array, this->source_array, reallocated_size * sizeof(float));

  char* check_interrogator = reinterpret_cast<char*>(this->check_array);
  for (std::size_t i = 0; i < this->m_size * sizeof(float) / sizeof(char); i++) {
    ASSERT_EQ(check_interrogator[i], 1);
  }

  for (std::size_t i = this->m_size * sizeof(float) / sizeof(char); i < reallocated_size * sizeof(float) / sizeof(char);
       i++) {
    ASSERT_EQ(check_interrogator[i], 2);
  }
}

TYPED_TEST(ReallocateTest, RealocateNull)
{
  umpire::ResourceManager& rm = umpire::ResourceManager::getInstance();

  rm.setDefaultAllocator(*this->source_allocator);

  const std::size_t reallocated_size = (this->m_size + 50);

  void* null_array = nullptr;

  float* reallocated_array = static_cast<float*>(rm.reallocate(null_array, reallocated_size * sizeof(float)));

  ASSERT_EQ(this->source_allocator->getSize(reallocated_array), reallocated_size * sizeof(float));

  rm.deallocate(reallocated_array);
  rm.setDefaultAllocator(rm.getAllocator("HOST"));
}

TYPED_TEST(ReallocateTest, ReallocateNullZero)
{
  umpire::ResourceManager& rm = umpire::ResourceManager::getInstance();

  rm.setDefaultAllocator(*this->source_allocator);
  // nullptr, zero size
  const std::size_t reallocated_zero_size{0};
  const std::size_t reallocated_size_1{this->m_size + 50};
  const std::size_t reallocated_size_2{this->m_size + 100};

  float* reallocated_array{nullptr};

  // nullptr, zero size
  reallocated_array =
      static_cast<float*>(rm.reallocate(static_cast<void*>(reallocated_array), reallocated_zero_size * sizeof(float)));

  ASSERT_EQ(this->source_allocator->getSize(reallocated_array), reallocated_zero_size * sizeof(float));
  rm.deallocate(reallocated_array);
  reallocated_array = nullptr;

  // nullptr, non-zero size
  reallocated_array =
      static_cast<float*>(rm.reallocate(static_cast<void*>(reallocated_array), reallocated_size_1 * sizeof(float)));

  ASSERT_EQ(this->source_allocator->getSize(reallocated_array), reallocated_size_1 * sizeof(float));

  // valid ptr (size > 0), non-zero increment
  reallocated_array =
      static_cast<float*>(rm.reallocate(static_cast<void*>(reallocated_array), reallocated_size_2 * sizeof(float)));

  ASSERT_EQ(this->source_allocator->getSize(reallocated_array), reallocated_size_2 * sizeof(float));

  // valid ptr (size > 0), non-zero decrement
  reallocated_array =
      static_cast<float*>(rm.reallocate(static_cast<void*>(reallocated_array), reallocated_size_1 * sizeof(float)));

  ASSERT_EQ(this->source_allocator->getSize(reallocated_array), reallocated_size_1 * sizeof(float));

  // valid ptr (size > 0), zero size
  reallocated_array =
      static_cast<float*>(rm.reallocate(static_cast<void*>(reallocated_array), reallocated_zero_size * sizeof(float)));

  ASSERT_EQ(this->source_allocator->getSize(reallocated_array), reallocated_zero_size * sizeof(float));

  // valid ptr (size == 0), non-zero size
  reallocated_array =
      static_cast<float*>(rm.reallocate(static_cast<void*>(reallocated_array), reallocated_size_1 * sizeof(float)));

  ASSERT_EQ(this->source_allocator->getSize(reallocated_array), reallocated_size_1 * sizeof(float));

  rm.deallocate(reallocated_array);
  rm.setDefaultAllocator(rm.getAllocator("HOST"));
}

TYPED_TEST(ReallocateTest, ReallocateNullZeroWithAllocator)
{
  umpire::ResourceManager& rm = umpire::ResourceManager::getInstance();

  // nullptr, zero size
  const std::size_t reallocated_zero_size{0};
  const std::size_t reallocated_size_1{this->m_size + 50};
  const std::size_t reallocated_size_2{this->m_size + 100};

  float* reallocated_array{nullptr};

  // nullptr, zero size
  reallocated_array = static_cast<float*>(rm.reallocate(
      static_cast<void*>(reallocated_array), reallocated_zero_size * sizeof(float), *this->source_allocator));

  ASSERT_EQ(this->source_allocator->getSize(reallocated_array), reallocated_zero_size * sizeof(float));
  rm.deallocate(reallocated_array);
  reallocated_array = nullptr;

  // nullptr, non-zero size
  reallocated_array = static_cast<float*>(rm.reallocate(static_cast<void*>(reallocated_array),
                                                        reallocated_size_1 * sizeof(float), *this->source_allocator));

  ASSERT_EQ(this->source_allocator->getSize(reallocated_array), reallocated_size_1 * sizeof(float));

  // valid ptr, non-zero increment
  reallocated_array = static_cast<float*>(rm.reallocate(static_cast<void*>(reallocated_array),
                                                        reallocated_size_2 * sizeof(float), *this->source_allocator));

  ASSERT_EQ(this->source_allocator->getSize(reallocated_array), reallocated_size_2 * sizeof(float));

  // valid ptr, non-zero decrement
  reallocated_array = static_cast<float*>(rm.reallocate(static_cast<void*>(reallocated_array),
                                                        reallocated_size_1 * sizeof(float), *this->source_allocator));

  ASSERT_EQ(this->source_allocator->getSize(reallocated_array), reallocated_size_1 * sizeof(float));

  // valid ptr, zero size
  reallocated_array = static_cast<float*>(rm.reallocate(
      static_cast<void*>(reallocated_array), reallocated_zero_size * sizeof(float), *this->source_allocator));

  ASSERT_EQ(this->source_allocator->getSize(reallocated_array), reallocated_zero_size * sizeof(float));

  rm.deallocate(reallocated_array);
}

TYPED_TEST(ReallocateTest, ReallocateNullWithAllocator)
{
  umpire::ResourceManager& rm = umpire::ResourceManager::getInstance();

  const std::size_t reallocated_size = (this->m_size + 50);

  void* null_array = nullptr;

  float* reallocated_array =
      static_cast<float*>(rm.reallocate(null_array, reallocated_size * sizeof(float), *this->source_allocator));

  ASSERT_EQ(this->source_allocator->getSize(reallocated_array), reallocated_size * sizeof(float));

  rm.deallocate(reallocated_array);
}

TYPED_TEST(ReallocateTest, ReallocateWithAllocator)
{
  umpire::ResourceManager& rm = umpire::ResourceManager::getInstance();

  const std::size_t reallocated_size = (this->m_size + 50);

  float* reallocated_array =
      static_cast<float*>(rm.reallocate(this->source_array, reallocated_size * sizeof(float), *this->source_allocator));

  ASSERT_EQ(this->source_allocator->getSize(reallocated_array), reallocated_size * sizeof(float));

  rm.deallocate(reallocated_array);
  this->source_array = nullptr;
}

TYPED_TEST(ReallocateTest, ReallocateWithAllocatorFail)
{
  if (this->source_allocator->getId() == this->dest_allocator->getId()) {
    SUCCEED();
  } else {
    umpire::ResourceManager& rm = umpire::ResourceManager::getInstance();
    ASSERT_THROW(rm.reallocate(this->source_array, this->m_size, *this->dest_allocator), umpire::runtime_error);
  }
}

template <typename T>
class MoveTest : public OperationTest<T> {
};

TYPED_TEST_SUITE(MoveTest, AllTestTypes, );

TYPED_TEST(MoveTest, Move)
{
  auto& rm = umpire::ResourceManager::getInstance();

  // this works because source should always be host!
  for (std::size_t i = 0; i < this->m_size; i++) {
    this->source_array[i] = static_cast<float>(i);
  }

  float* moved_array = static_cast<float*>(rm.move(this->source_array, *this->dest_allocator));

  if (this->dest_allocator->getAllocationStrategy() == this->source_allocator->getAllocationStrategy()) {
    ASSERT_EQ(moved_array, this->source_array);
  }

  rm.copy(this->check_array, moved_array);

  for (std::size_t i = 0; i < this->m_size; i++) {
    ASSERT_FLOAT_EQ(this->check_array[i], static_cast<float>(i));
  }

  this->dest_allocator->deallocate(moved_array);
  this->source_array = nullptr;
}

#if defined(UMPIRE_ENABLE_CUDA) || defined(UMPIRE_ENABLE_HIP)
template <typename T>
class AdviceTest : public OperationTest<T> {
};

using AdviceTypes =
    camp::cartesian_product<camp::list<um_resource_tag>, camp::list<host_resource_tag, device_resource_tag>,
                            camp::list<NullStrategy>, camp::list<NullStrategy>>;

using AdviceTestTypes = Test<AdviceTypes>::Types;

TYPED_TEST_SUITE(AdviceTest, AdviceTestTypes, );

TYPED_TEST(AdviceTest, ReadMostly)
{
  auto& op_registry = umpire::op::MemoryOperationRegistry::getInstance();
  auto strategy = this->source_allocator->getAllocationStrategy();

  int device = 0;

  auto m_advice_operation = op_registry.find("SET_READ_MOSTLY", strategy, strategy);

  if (this->dest_allocator->getPlatform() == umpire::Platform::host) {
#if defined(UMPIRE_ENABLE_CUDA)
    device = cudaCpuDeviceId;
#elif defined(UMPIRE_ENABLE_HIP)
    device = hipCpuDeviceId;
#endif
  }

  ASSERT_NO_THROW({ m_advice_operation->apply(this->source_array, nullptr, device, this->m_size); });
}

TYPED_TEST(AdviceTest, PreferredLocation)
{
  auto& op_registry = umpire::op::MemoryOperationRegistry::getInstance();
  auto strategy = this->source_allocator->getAllocationStrategy();

  int device = 0;

  auto m_advice_operation = op_registry.find("SET_PREFERRED_LOCATION", strategy, strategy);

  if (this->dest_allocator->getPlatform() == umpire::Platform::host) {
#if defined(UMPIRE_ENABLE_CUDA)
    device = cudaCpuDeviceId;
#elif defined(UMPIRE_ENABLE_HIP)
    device = hipCpuDeviceId;
#endif
  }

  ASSERT_NO_THROW({ m_advice_operation->apply(this->source_array, nullptr, device, this->m_size); });
}

TYPED_TEST(AdviceTest, AccessedBy)
{
  auto& op_registry = umpire::op::MemoryOperationRegistry::getInstance();
  auto strategy = this->source_allocator->getAllocationStrategy();

  int device = 0;

  auto m_advice_operation = op_registry.find("SET_ACCESSED_BY", strategy, strategy);

  if (this->dest_allocator->getPlatform() == umpire::Platform::host) {
#if defined(UMPIRE_ENABLE_CUDA)
    device = cudaCpuDeviceId;
#elif defined(UMPIRE_ENABLE_HIP)
    device = hipCpuDeviceId;
#endif
  }

  ASSERT_NO_THROW({ m_advice_operation->apply(this->source_array, nullptr, device, this->m_size); });
}

TEST(AsyncTest, Copy)
{
  auto resource = camp::resources::Resource{resource_type{}};
  auto& rm = umpire::ResourceManager::getInstance();

  constexpr std::size_t size = 1024;

  auto host_alloc = rm.getAllocator("HOST");
  auto device_alloc = rm.getAllocator("DEVICE");

  float* source_array = static_cast<float*>(host_alloc.allocate(size * sizeof(float)));
  float* check_array = static_cast<float*>(host_alloc.allocate(size * sizeof(float)));

  float* dest_array = static_cast<float*>(device_alloc.allocate(size * sizeof(float)));

  for (std::size_t i = 0; i < size; i++) {
    source_array[i] = static_cast<float>(i);
  }

  camp::resources::Event event = rm.copy(dest_array, source_array, resource);
  event = rm.copy(check_array, dest_array, resource);

  event.wait();

  for (std::size_t i = 0; i < size; i++) {
    ASSERT_FLOAT_EQ(source_array[i], check_array[i]);
  }

  host_alloc.deallocate(source_array);
  host_alloc.deallocate(check_array);
  device_alloc.deallocate(dest_array);
}

TEST(AsyncTest, Memset)
{
  auto resource = camp::resources::Resource{resource_type{}};
  auto& rm = umpire::ResourceManager::getInstance();

  constexpr std::size_t size = 1024;

  auto host_alloc = rm.getAllocator("HOST");
  auto device_alloc = rm.getAllocator("DEVICE");

  float* source_array = static_cast<float*>(device_alloc.allocate(size * sizeof(float)));
  float* check_array = static_cast<float*>(host_alloc.allocate(size * sizeof(float)));

  auto event_proxy = rm.memset(source_array, 0, resource);
  camp::resources::Event event = rm.copy(check_array, source_array, resource);

  event.wait();

  for (std::size_t i = 0; i < size; i++) {
    ASSERT_FLOAT_EQ(0, check_array[i]);
  }

  device_alloc.deallocate(source_array);
  host_alloc.deallocate(check_array);
}

TEST(AsyncTest, Reallocate)
{
  auto resource = camp::resources::Resource{resource_type{}};
  auto& rm = umpire::ResourceManager::getInstance();

  constexpr std::size_t size = 1024;

  auto device_alloc = rm.getAllocator("DEVICE");
  float* source_array = static_cast<float*>(device_alloc.allocate(size * sizeof(float)));
  source_array = static_cast<float*>(rm.reallocate(source_array, 50, resource));

  resource.get_event().wait();

  device_alloc.deallocate(source_array);
}
#endif

#if defined(UMPIRE_ENABLE_CUDA)
TEST(AsyncTest, Prefetch)
{
  auto resource = camp::resources::Resource{resource_type{}};
  auto& rm = umpire::ResourceManager::getInstance();

  constexpr std::size_t size = 1024;

  auto alloc = rm.getAllocator("UM");
  float* array = static_cast<float*>(alloc.allocate(size * sizeof(float)));

  ASSERT_NE(array, nullptr);

  // Prefetch to device 0
  camp::resources::Event event = rm.prefetch(array, 0, resource);
  event.wait();

  alloc.deallocate(array);
}
#endif

#if (defined(UMPIRE_ENABLE_CUDA) || defined(UMPIRE_ENABLE_HIP)) && defined(UMPIRE_ENABLE_CONST)
TEST(ConstDeviceMemoryTest, SetConstDeviceMemory)
{
  auto& rm = umpire::ResourceManager::getInstance();
  auto const_allocator = rm.getAllocator("DEVICE_CONST");

  static constexpr int N = 4;
  static constexpr int BYTESIZE = N * sizeof(int);
  static constexpr int TEST_VAL = 42;

  auto host_allocator = rm.getAllocator("HOST");
  int* HOST_DATA = static_cast<int*>(host_allocator.allocate(BYTESIZE));

  for (int i = 0; i < N; ++i) {
    HOST_DATA[i] = TEST_VAL;
  }

  int* A_d = static_cast<int*>(const_allocator.allocate(BYTESIZE));
  EXPECT_TRUE(A_d != nullptr);
  rm.copy(A_d, HOST_DATA, BYTESIZE);

  host_allocator.deallocate(HOST_DATA);
  const_allocator.deallocate(A_d);
}
#endif
