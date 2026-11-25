//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#include <cstring>

#include "gtest/gtest.h"
#include "umpire/Allocator.hpp"
#include "umpire/ResourceManager.hpp"
#include "umpire/op.hpp"

// Host prefetch tests - these are mostly no-ops but ensure API compatibility
TEST(Prefetch, HostPrefetchNoOp)
{
  constexpr std::size_t size = 1024;

  auto& rm = umpire::ResourceManager::getInstance();
  auto allocator = rm.getAllocator("HOST");

  void* ptr = allocator.allocate(size);

  std::memset(ptr, 0x42, size);

  // Prefetch should be a no-op for host memory, but should not crash
  EXPECT_NO_THROW(umpire::prefetch(ptr, 0, size));

  unsigned char* byte_ptr = static_cast<unsigned char*>(ptr);
  for (std::size_t i = 0; i < size; ++i) {
    ASSERT_EQ(byte_ptr[i], 0x42) << "Host prefetch modified data at byte " << i;
  }

  allocator.deallocate(ptr);
}

TEST(Prefetch, HostPrefetchExplicit)
{
  constexpr std::size_t size = 1024;

  auto& rm = umpire::ResourceManager::getInstance();
  auto allocator = rm.getAllocator("HOST");

  void* ptr = allocator.allocate(size);

  std::memset(ptr, 0x55, size);

  EXPECT_NO_THROW(umpire::prefetch<umpire::host>(ptr, 0, size));

  unsigned char* byte_ptr = static_cast<unsigned char*>(ptr);
  for (std::size_t i = 0; i < size; ++i) {
    ASSERT_EQ(byte_ptr[i], 0x55) << "Explicit host prefetch modified data at byte " << i;
  }

  allocator.deallocate(ptr);
}

TEST(Prefetch, ZeroSizePrefetch)
{
  auto& rm = umpire::ResourceManager::getInstance();
  auto allocator = rm.getAllocator("HOST");

  void* ptr = allocator.allocate(1024);

  EXPECT_NO_THROW(umpire::prefetch(ptr, 0, 0));

  allocator.deallocate(ptr);
}

#if defined(UMPIRE_ENABLE_CUDA)
TEST(Prefetch, CudaPrefetchToHost)
{
  constexpr std::size_t size = 1024;

  auto& rm = umpire::ResourceManager::getInstance();

  try {
    auto um_allocator = rm.getAllocator("UM");
    auto host_allocator = rm.getAllocator("HOST");

    unsigned char* um_ptr = static_cast<unsigned char*>(um_allocator.allocate(size));

    unsigned char* host_ptr = static_cast<unsigned char*>(host_allocator.allocate(size));

    std::memset(um_ptr, 0x77, size);

    // Prefetch to host (CPU device ID is typically -1 or cudaCpuDeviceId)
    EXPECT_NO_THROW(umpire::prefetch(um_ptr, -1, size));

    std::memcpy(host_ptr, um_ptr, size);

    for (std::size_t i = 0; i < size; ++i) {
      ASSERT_EQ(host_ptr[i], 0x77) << "CUDA prefetch corrupted data at byte " << i;
    }

    um_allocator.deallocate(um_ptr);
    host_allocator.deallocate(host_ptr);
  } catch (const std::runtime_error& e) {
    GTEST_SKIP() << "Unified Memory not available: " << e.what();
  }
}

TEST(Prefetch, CudaPrefetchToDevice)
{
  constexpr std::size_t size = 1024;

  auto& rm = umpire::ResourceManager::getInstance();

  try {
    auto um_allocator = rm.getAllocator("UM");
    auto host_allocator = rm.getAllocator("HOST");

    unsigned char* um_ptr = static_cast<unsigned char*>(um_allocator.allocate(size));

    unsigned char* host_ptr = static_cast<unsigned char*>(host_allocator.allocate(size));

    std::memset(um_ptr, 0x88, size);

    EXPECT_NO_THROW(umpire::prefetch(um_ptr, 0, size));

    std::memcpy(host_ptr, um_ptr, size);

    for (std::size_t i = 0; i < size; ++i) {
      ASSERT_EQ(host_ptr[i], 0x88) << "CUDA device prefetch corrupted data at byte " << i;
    }

    um_allocator.deallocate(um_ptr);
    host_allocator.deallocate(host_ptr);
  } catch (const std::runtime_error& e) {
    GTEST_SKIP() << "Unified Memory not available: " << e.what();
  }
}

TEST(Prefetch, ExplicitCudaPrefetch)
{
  constexpr std::size_t size = 1024;

  auto& rm = umpire::ResourceManager::getInstance();

  try {
    auto um_allocator = rm.getAllocator("UM");
    auto host_allocator = rm.getAllocator("HOST");

    unsigned char* um_ptr = static_cast<unsigned char*>(um_allocator.allocate(size));

    unsigned char* host_ptr = static_cast<unsigned char*>(host_allocator.allocate(size));

    std::memset(um_ptr, 0x99, size);

    EXPECT_NO_THROW(umpire::prefetch<umpire::cuda>(um_ptr, -1, size));

    std::memcpy(host_ptr, um_ptr, size);

    for (std::size_t i = 0; i < size; ++i) {
      ASSERT_EQ(host_ptr[i], 0x99) << "Explicit CUDA prefetch corrupted data at byte " << i;
    }

    um_allocator.deallocate(um_ptr);
    host_allocator.deallocate(host_ptr);
  } catch (const std::runtime_error& e) {
    GTEST_SKIP() << "Unified Memory not available: " << e.what();
  }
}
#endif // UMPIRE_ENABLE_CUDA

#if defined(UMPIRE_ENABLE_HIP)
TEST(Prefetch, HipPrefetchToHost)
{
  constexpr std::size_t size = 1024;

  auto& rm = umpire::ResourceManager::getInstance();

  try {
    auto um_allocator = rm.getAllocator("UM");
    auto host_allocator = rm.getAllocator("HOST");

    unsigned char* um_ptr = static_cast<unsigned char*>(um_allocator.allocate(size));

    unsigned char* host_ptr = static_cast<unsigned char*>(host_allocator.allocate(size));

    std::memset(um_ptr, 0xAA, size);

    // Prefetch to host (CPU device ID is typically -1 or hipCpuDeviceId)
    EXPECT_NO_THROW(umpire::prefetch(um_ptr, -1, size));

    std::memcpy(host_ptr, um_ptr, size);

    for (std::size_t i = 0; i < size; ++i) {
      ASSERT_EQ(host_ptr[i], 0xAA) << "HIP prefetch corrupted data at byte " << i;
    }

    um_allocator.deallocate(um_ptr);
    host_allocator.deallocate(host_ptr);
  } catch (const std::runtime_error& e) {
    GTEST_SKIP() << "Unified Memory not available: " << e.what();
  }
}

TEST(Prefetch, HipPrefetchToDevice)
{
  constexpr std::size_t size = 1024;

  auto& rm = umpire::ResourceManager::getInstance();

  try {
    auto um_allocator = rm.getAllocator("UM");
    auto host_allocator = rm.getAllocator("HOST");

    unsigned char* um_ptr = static_cast<unsigned char*>(um_allocator.allocate(size));

    unsigned char* host_ptr = static_cast<unsigned char*>(host_allocator.allocate(size));

    std::memset(um_ptr, 0xBB, size);

    EXPECT_NO_THROW(umpire::prefetch(um_ptr, 0, size));

    std::memcpy(host_ptr, um_ptr, size);

    for (std::size_t i = 0; i < size; ++i) {
      ASSERT_EQ(host_ptr[i], 0xBB) << "HIP device prefetch corrupted data at byte " << i;
    }

    um_allocator.deallocate(um_ptr);
    host_allocator.deallocate(host_ptr);
  } catch (const std::runtime_error& e) {
    GTEST_SKIP() << "Unified Memory not available: " << e.what();
  }
}

TEST(Prefetch, ExplicitHipPrefetch)
{
  constexpr std::size_t size = 1024;

  auto& rm = umpire::ResourceManager::getInstance();

  try {
    auto um_allocator = rm.getAllocator("UM");
    auto host_allocator = rm.getAllocator("HOST");

    unsigned char* um_ptr = static_cast<unsigned char*>(um_allocator.allocate(size));

    unsigned char* host_ptr = static_cast<unsigned char*>(host_allocator.allocate(size));

    std::memset(um_ptr, 0xCC, size);

    EXPECT_NO_THROW(umpire::prefetch<umpire::hip>(um_ptr, -1, size));

    std::memcpy(host_ptr, um_ptr, size);

    for (std::size_t i = 0; i < size; ++i) {
      ASSERT_EQ(host_ptr[i], 0xCC) << "Explicit HIP prefetch corrupted data at byte " << i;
    }

    um_allocator.deallocate(um_ptr);
    host_allocator.deallocate(host_ptr);
  } catch (const std::runtime_error& e) {
    GTEST_SKIP() << "Unified Memory not available: " << e.what();
  }
}
#endif // UMPIRE_ENABLE_HIP

//------------------------------------------------------------------------------
// Platform-by-value overload tests
//------------------------------------------------------------------------------

TEST(Prefetch, ExplicitPlatformHost)
{
  constexpr std::size_t size = 512;

  auto& rm = umpire::ResourceManager::getInstance();
  auto allocator = rm.getAllocator("HOST");

  void* ptr = allocator.allocate(size);

  std::memset(ptr, 0xEE, size);

  EXPECT_NO_THROW(umpire::prefetch(camp::resources::Platform::host, ptr, 0, size));

  unsigned char* byte_ptr = static_cast<unsigned char*>(ptr);
  for (std::size_t i = 0; i < size; ++i) {
    ASSERT_EQ(byte_ptr[i], 0xEE) << "Prefetch modified data at byte " << i;
  }

  allocator.deallocate(ptr);
}

TEST(Prefetch, ExplicitPlatformHostAsync)
{
  constexpr std::size_t size = 512;

  auto& rm = umpire::ResourceManager::getInstance();
  auto allocator = rm.getAllocator("HOST");

  void* ptr = allocator.allocate(size);

  std::memset(ptr, 0xFF, size);

  camp::resources::Resource host_ctx{camp::resources::Host{}};

  auto event = umpire::prefetch(camp::resources::Platform::host, ptr, 0, size, host_ctx);

  static_cast<camp::resources::Event>(event).wait();

  unsigned char* byte_ptr = static_cast<unsigned char*>(ptr);
  for (std::size_t i = 0; i < size; ++i) {
    ASSERT_EQ(byte_ptr[i], 0xFF) << "Async prefetch modified data at byte " << i;
  }

  allocator.deallocate(ptr);
}

#if defined(UMPIRE_ENABLE_CUDA)
TEST(Prefetch, ExplicitPlatformCuda)
{
  constexpr std::size_t size = 512;

  auto& rm = umpire::ResourceManager::getInstance();

  try {
    auto um_allocator = rm.getAllocator("UM");
    auto host_allocator = rm.getAllocator("HOST");

    unsigned char* um_ptr = static_cast<unsigned char*>(um_allocator.allocate(size));
    unsigned char* host_ptr = static_cast<unsigned char*>(host_allocator.allocate(size));

    std::memset(um_ptr, 0xDD, size);

    EXPECT_NO_THROW(umpire::prefetch(camp::resources::Platform::cuda, um_ptr, 0, size));

    std::memcpy(host_ptr, um_ptr, size);

    for (std::size_t i = 0; i < size; ++i) {
      ASSERT_EQ(host_ptr[i], 0xDD) << "Prefetch corrupted data at byte " << i;
    }

    um_allocator.deallocate(um_ptr);
    host_allocator.deallocate(host_ptr);
  } catch (const std::runtime_error& e) {
    GTEST_SKIP() << "Unified Memory not available: " << e.what();
  }
}

TEST(Prefetch, ExplicitPlatformCudaAsync)
{
  constexpr std::size_t size = 512;

  auto& rm = umpire::ResourceManager::getInstance();

  try {
    auto um_allocator = rm.getAllocator("UM");
    auto host_allocator = rm.getAllocator("HOST");

    unsigned char* um_ptr = static_cast<unsigned char*>(um_allocator.allocate(size));
    unsigned char* host_ptr = static_cast<unsigned char*>(host_allocator.allocate(size));

    std::memset(um_ptr, 0xBC, size);

    camp::resources::Resource cuda_ctx{camp::resources::Cuda{}};

    auto event = umpire::prefetch(camp::resources::Platform::cuda, um_ptr, -1, size, cuda_ctx);

    static_cast<camp::resources::Event>(event).wait();

    std::memcpy(host_ptr, um_ptr, size);

    for (std::size_t i = 0; i < size; ++i) {
      ASSERT_EQ(host_ptr[i], 0xBC) << "Async prefetch corrupted data at byte " << i;
    }

    um_allocator.deallocate(um_ptr);
    host_allocator.deallocate(host_ptr);
  } catch (const std::runtime_error& e) {
    GTEST_SKIP() << "Unified Memory not available: " << e.what();
  }
}
#endif // UMPIRE_ENABLE_CUDA