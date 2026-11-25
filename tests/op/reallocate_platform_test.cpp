//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#include "gtest/gtest.h"
#include "umpire/ResourceManager.hpp"
#include "umpire/op.hpp"
#include "umpire/resource/NullMemoryResource.hpp"
#include "umpire/strategy/QuickPool.hpp"

namespace {

// Reserve inaccessible memory with a non-host platform without requiring a GPU.
class UndefinedPlatformResource : public umpire::resource::NullMemoryResource {
 public:
  UndefinedPlatformResource(const std::string& name, int id)
      : NullMemoryResource(umpire::Platform::undefined, name, id, {})
  {
  }
};

class ReallocatePlatform : public ::testing::TestWithParam<bool> {
 protected:
  umpire::ResourceManager& rm = umpire::ResourceManager::getInstance();
  umpire::Allocator original_default = rm.getDefaultAllocator();

  umpire::Allocator other_allocator()
  {
    static auto allocator = rm.makeAllocator<UndefinedPlatformResource>("reallocate_platform_undefined");
    return allocator;
  }

  void TearDown() override
  {
    rm.setDefaultAllocator(original_default);
  }
};

TEST_P(ReallocatePlatform, NullTypedPointerUsesRequestedHostPlatform)
{
  // This resource has an undefined platform and reserves inaccessible memory.
  // Explicit HOST must override this otherwise valid global default.
  rm.setDefaultAllocator(other_allocator());
  int* ptr = nullptr;
  if (GetParam()) {
    camp::resources::Resource ctx{camp::resources::Host{}};
    auto event = umpire::reallocate(camp::resources::Platform::host, &ptr, 4, ctx);
    static_cast<camp::resources::Event>(event).wait();
  } else {
    umpire::reallocate(camp::resources::Platform::host, &ptr, 4);
  }

  EXPECT_EQ(rm.getAllocator(ptr).getPlatform(), camp::resources::Platform::host);
  EXPECT_EQ(rm.getSize(ptr), 4 * sizeof(int));
  rm.deallocate(ptr);
}

TEST_P(ReallocatePlatform, NullVoidPointerUsesRequestedHostPlatform)
{
  rm.setDefaultAllocator(other_allocator());
  void* ptr = nullptr;
  if (GetParam()) {
    camp::resources::Resource ctx{camp::resources::Host{}};
    auto event = umpire::reallocate(camp::resources::Platform::host, &ptr, 7, ctx);
    static_cast<camp::resources::Event>(event).wait();
  } else {
    umpire::reallocate(camp::resources::Platform::host, &ptr, 7);
  }

  EXPECT_EQ(rm.getAllocator(ptr).getPlatform(), camp::resources::Platform::host);
  EXPECT_EQ(rm.getSize(ptr), 7u);
  rm.deallocate(ptr);
}

TEST_P(ReallocatePlatform, PreservesCustomAllocatorAndData)
{
  auto pool = rm.makeAllocator<umpire::strategy::QuickPool>(
      GetParam() ? "reallocate_platform_async_pool" : "reallocate_platform_sync_pool", rm.getAllocator("HOST"), 1024,
      1024);
  rm.setDefaultAllocator(pool);
  int* ptr = nullptr;
  camp::resources::Resource ctx{camp::resources::Host{}};
  auto resize = [&](std::size_t count) {
    if (GetParam()) {
      auto event = umpire::reallocate(camp::resources::Platform::host, &ptr, count, ctx);
      static_cast<camp::resources::Event>(event).wait();
    } else {
      umpire::reallocate(camp::resources::Platform::host, &ptr, count);
    }
  };

  resize(4);
  EXPECT_EQ(rm.getAllocator(ptr).getId(), pool.getId());
  ptr[0] = 42;
  // A non-null pointer must retain its allocator even after the default changes.
  rm.setDefaultAllocator(rm.getAllocator("HOST"));
  resize(8);
  EXPECT_EQ(rm.getAllocator(ptr).getId(), pool.getId());
  EXPECT_EQ(ptr[0], 42);
  EXPECT_EQ(rm.getSize(ptr), 8 * sizeof(int));
  rm.deallocate(ptr);
  pool.release();
}

TEST_P(ReallocatePlatform, RejectsExistingPointerOnDifferentPlatform)
{
  void* ptr = other_allocator().allocate(16);
  void* original = ptr;
  if (GetParam()) {
    camp::resources::Resource ctx{camp::resources::Host{}};
    EXPECT_THROW(umpire::reallocate(camp::resources::Platform::host, &ptr, 0, ctx), umpire::runtime_error);
  } else {
    EXPECT_THROW(umpire::reallocate(camp::resources::Platform::host, &ptr, 0), umpire::runtime_error);
  }

  EXPECT_EQ(ptr, original);
  EXPECT_EQ(rm.getSize(ptr), 16u);
  rm.deallocate(ptr);
}

TEST_P(ReallocatePlatform, RejectsUnsupportedPlatformWithoutAllocating)
{
  void* ptr = nullptr;
  if (GetParam()) {
    camp::resources::Resource ctx{camp::resources::Host{}};
    EXPECT_THROW(umpire::reallocate(camp::resources::Platform::undefined, &ptr, 16, ctx), umpire::runtime_error);
  } else {
    EXPECT_THROW(umpire::reallocate(camp::resources::Platform::undefined, &ptr, 16), umpire::runtime_error);
  }
  EXPECT_EQ(ptr, nullptr);
}

TEST_P(ReallocatePlatform, AutoDispatchNullPointerRetainsGlobalDefault)
{
  auto default_allocator = other_allocator();
  rm.setDefaultAllocator(default_allocator);
  void* ptr = nullptr;
  if (GetParam()) {
    camp::resources::Resource ctx{camp::resources::Host{}};
    auto event = umpire::reallocate(&ptr, 16, ctx);
    static_cast<camp::resources::Event>(event).wait();
  } else {
    umpire::reallocate(&ptr, 16);
  }

  EXPECT_EQ(rm.getAllocator(ptr).getId(), default_allocator.getId());
  rm.deallocate(ptr);
}

TEST_P(ReallocatePlatform, NullPointerUsesRequestedDevicePlatform)
{
  auto device = rm.tryGetAllocator("DEVICE");
  if (!device) {
    GTEST_SKIP() << "No device allocator available";
  }
  rm.setDefaultAllocator(rm.getAllocator("HOST"));
  auto platform = device->getPlatform();
  // Host{} placeholder: camp 2025.12 has no default Resource constructor.
  camp::resources::Resource ctx{camp::resources::Host{}};
  switch (platform) {
#if defined(UMPIRE_ENABLE_CUDA)
    case camp::resources::Platform::cuda:
      ctx = camp::resources::Cuda{};
      break;
#endif
#if defined(UMPIRE_ENABLE_HIP)
    case camp::resources::Platform::hip:
      ctx = camp::resources::Hip{};
      break;
#endif
#if defined(UMPIRE_ENABLE_SYCL)
    case camp::resources::Platform::sycl:
      ctx = camp::resources::Sycl{};
      break;
#endif
#if defined(UMPIRE_ENABLE_OPENMP_TARGET)
    case camp::resources::Platform::omp_target:
      ctx = camp::resources::Omp{};
      break;
#endif
    default:
      FAIL() << "Unexpected DEVICE allocator platform";
  }

  int* ptr = nullptr;
  if (GetParam()) {
    auto event = umpire::reallocate(platform, &ptr, 4, ctx);
    static_cast<camp::resources::Event>(event).wait();
  } else {
    umpire::reallocate(platform, &ptr, 4);
  }
  EXPECT_EQ(rm.getAllocator(ptr).getPlatform(), platform);
  EXPECT_EQ(rm.getSize(ptr), 4 * sizeof(int));
  rm.deallocate(ptr);
}

INSTANTIATE_TEST_SUITE_P(SyncAndAsync, ReallocatePlatform, ::testing::Bool());

} // namespace
