//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-25, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#include <algorithm>
#include <cstring>
#include <random>

#include "gtest/gtest.h"
#include "umpire/Allocator.hpp"
#include "umpire/ResourceManager.hpp"
#include "umpire/op.hpp"

// Host advise tests - these should throw runtime errors since host doesn't support memory advice
TEST(Advise, HostAdviseThrows)
{
  constexpr std::size_t size = 1024;

  auto& rm = umpire::ResourceManager::getInstance();
  auto allocator = rm.getAllocator("HOST");

  // Allocate host buffer
  void* ptr = allocator.allocate(size);

  // Fill with test data
  std::memset(ptr, 0x33, size);

  // Advise operations should throw runtime errors for host memory
  EXPECT_THROW(umpire::set_accessed_by(ptr, 0, size), std::runtime_error);
  EXPECT_THROW(umpire::set_preferred_location(ptr, 0, size), std::runtime_error);
  EXPECT_THROW(umpire::set_read_mostly(ptr, 0, size), std::runtime_error);
  EXPECT_THROW(umpire::unset_accessed_by(ptr, 0, size), std::runtime_error);
  EXPECT_THROW(umpire::unset_preferred_location(ptr, 0, size), std::runtime_error);
  EXPECT_THROW(umpire::unset_read_mostly(ptr, 0, size), std::runtime_error);

  // Data should be unchanged after failed operations
  unsigned char* byte_ptr = static_cast<unsigned char*>(ptr);
  for (std::size_t i = 0; i < size; ++i) {
    ASSERT_EQ(byte_ptr[i], 0x33) << "Host advise modified data at byte " << i;
  }

  // Cleanup
  allocator.deallocate(ptr);
}

TEST(Advise, HostAdviseExplicit)
{
  constexpr std::size_t size = 1024;

  auto& rm = umpire::ResourceManager::getInstance();
  auto allocator = rm.getAllocator("HOST");

  // Allocate host buffer
  void* ptr = allocator.allocate(size);

  // Fill with test data
  std::memset(ptr, 0x66, size);

  // Explicit host advise - should not compile due to SFINAE constraints
  // These calls should fail at compile time:
  // umpire::set_accessed_by<umpire::resource::host_platform>(ptr, 0, size);
  // umpire::set_preferred_location<umpire::resource::host_platform>(ptr, 0, size);
  // umpire::set_read_mostly<umpire::resource::host_platform>(ptr, 0, size);

  // Data should be unchanged
  unsigned char* byte_ptr = static_cast<unsigned char*>(ptr);
  for (std::size_t i = 0; i < size; ++i) {
    ASSERT_EQ(byte_ptr[i], 0x66) << "Explicit host advise modified data at byte " << i;
  }

  // Cleanup
  allocator.deallocate(ptr);
}

TEST(Advise, ZeroSizeAdvise)
{
  auto& rm = umpire::ResourceManager::getInstance();
  auto allocator = rm.getAllocator("HOST");

  // Allocate host buffer
  void* ptr = allocator.allocate(1024);

  // Zero-size advise should still throw for host (platform doesn't support it regardless of size)
  EXPECT_THROW(umpire::set_accessed_by(ptr, 0, 0), std::runtime_error);
  EXPECT_THROW(umpire::set_preferred_location(ptr, 0, 0), std::runtime_error);
  EXPECT_THROW(umpire::set_read_mostly(ptr, 0, 0), std::runtime_error);

  // Cleanup
  allocator.deallocate(ptr);
}

#if defined(UMPIRE_ENABLE_CUDA)
TEST(Advise, CudaSetAccessedBy)
{
  constexpr std::size_t size = 1024;

  auto& rm = umpire::ResourceManager::getInstance();

  // Check if we have managed memory support
  try {
    auto um_allocator = rm.getAllocator("UM");
    auto host_allocator = rm.getAllocator("HOST");

    // Allocate managed memory
    void* um_ptr = um_allocator.allocate(size);

    // Allocate host buffer for verification
    unsigned char* host_ptr = static_cast<unsigned char*>(host_allocator.allocate(size));

    // Fill managed memory with test data
    std::memset(um_ptr, 0x44, size);

    // Set accessed by device 0 (should not crash or corrupt data)
    EXPECT_NO_THROW(umpire::set_accessed_by(um_ptr, 0, size));

    // Copy to host memory to verify data integrity
    std::memcpy(host_ptr, um_ptr, size);

    // Verify data is intact after advise
    for (std::size_t i = 0; i < size; ++i) {
      ASSERT_EQ(host_ptr[i], 0x44) << "CUDA set_accessed_by corrupted data at byte " << i;
    }

    // Test unsetting
    EXPECT_NO_THROW(umpire::unset_accessed_by(um_ptr, 0, size));

    // Cleanup
    um_allocator.deallocate(um_ptr);
    host_allocator.deallocate(host_ptr);
  } catch (const std::runtime_error& e) {
    // If UM allocator is not available, skip this test
    GTEST_SKIP() << "Unified Memory not available: " << e.what();
  }
}

TEST(Advise, CudaSetPreferredLocation)
{
  constexpr std::size_t size = 1024;

  auto& rm = umpire::ResourceManager::getInstance();

  // Check if we have managed memory support
  try {
    auto um_allocator = rm.getAllocator("UM");
    auto host_allocator = rm.getAllocator("HOST");

    // Allocate managed memory
    void* um_ptr = um_allocator.allocate(size);

    // Allocate host buffer for verification
    unsigned char* host_ptr = static_cast<unsigned char*>(host_allocator.allocate(size));

    // Fill managed memory with test data
    std::memset(um_ptr, 0x55, size);

    // Set preferred location to device 0
    EXPECT_NO_THROW(umpire::set_preferred_location(um_ptr, 0, size));

    // Copy to host memory to verify data integrity
    std::memcpy(host_ptr, um_ptr, size);

    // Verify data is intact after advise
    for (std::size_t i = 0; i < size; ++i) {
      ASSERT_EQ(host_ptr[i], 0x55) << "CUDA set_preferred_location corrupted data at byte " << i;
    }

    // Test unsetting
    EXPECT_NO_THROW(umpire::unset_preferred_location(um_ptr, 0, size));

    // Test setting preferred location to host (-1)
    EXPECT_NO_THROW(umpire::set_preferred_location(um_ptr, -1, size));

    // Cleanup
    um_allocator.deallocate(um_ptr);
    host_allocator.deallocate(host_ptr);
  } catch (const std::runtime_error& e) {
    // If UM allocator is not available, skip this test
    GTEST_SKIP() << "Unified Memory not available: " << e.what();
  }
}

TEST(Advise, CudaSetReadMostly)
{
  constexpr std::size_t size = 1024;

  auto& rm = umpire::ResourceManager::getInstance();

  // Check if we have managed memory support
  try {
    auto um_allocator = rm.getAllocator("UM");
    auto host_allocator = rm.getAllocator("HOST");

    // Allocate managed memory
    void* um_ptr = um_allocator.allocate(size);

    // Allocate host buffer for verification
    unsigned char* host_ptr = static_cast<unsigned char*>(host_allocator.allocate(size));

    // Fill managed memory with test data
    std::memset(um_ptr, 0x77, size);

    // Set read mostly hint
    EXPECT_NO_THROW(umpire::set_read_mostly(um_ptr, 0, size));

    // Copy to host memory to verify data integrity
    std::memcpy(host_ptr, um_ptr, size);

    // Verify data is intact after advise
    for (std::size_t i = 0; i < size; ++i) {
      ASSERT_EQ(host_ptr[i], 0x77) << "CUDA set_read_mostly corrupted data at byte " << i;
    }

    // Test unsetting
    EXPECT_NO_THROW(umpire::unset_read_mostly(um_ptr, 0, size));

    // Cleanup
    um_allocator.deallocate(um_ptr);
    host_allocator.deallocate(host_ptr);
  } catch (const std::runtime_error& e) {
    // If UM allocator is not available, skip this test
    GTEST_SKIP() << "Unified Memory not available: " << e.what();
  }
}

TEST(Advise, ExplicitCudaAdvise)
{
  constexpr std::size_t size = 1024;

  auto& rm = umpire::ResourceManager::getInstance();

  // Check if we have managed memory support
  try {
    auto um_allocator = rm.getAllocator("UM");
    auto host_allocator = rm.getAllocator("HOST");

    // Allocate managed memory
    void* um_ptr = um_allocator.allocate(size);

    // Allocate host buffer for verification
    unsigned char* host_ptr = static_cast<unsigned char*>(host_allocator.allocate(size));

    // Fill managed memory with test data
    std::memset(um_ptr, 0x88, size);

    // Explicit CUDA advise operations
    EXPECT_NO_THROW(umpire::set_accessed_by<umpire::cuda>(um_ptr, 0, size));
    EXPECT_NO_THROW(umpire::set_preferred_location<umpire::cuda>(um_ptr, 0, size));
    EXPECT_NO_THROW(umpire::set_read_mostly<umpire::cuda>(um_ptr, 0, size));

    // Copy to host memory to verify data integrity
    std::memcpy(host_ptr, um_ptr, size);

    // Verify data is intact after advise
    for (std::size_t i = 0; i < size; ++i) {
      ASSERT_EQ(host_ptr[i], 0x88) << "Explicit CUDA advise corrupted data at byte " << i;
    }

    // Cleanup
    um_allocator.deallocate(um_ptr);
    host_allocator.deallocate(host_ptr);
  } catch (const std::runtime_error& e) {
    // If UM allocator is not available, skip this test
    GTEST_SKIP() << "Unified Memory not available: " << e.what();
  }
}

#if defined(CUDA_VERSION) && CUDA_VERSION >= 8000
TEST(Advise, CudaCoarseGrain)
{
  constexpr std::size_t size = 1024;

  auto& rm = umpire::ResourceManager::getInstance();

  // Check if we have managed memory support
  try {
    auto um_allocator = rm.getAllocator("UM");
    auto host_allocator = rm.getAllocator("HOST");

    // Allocate managed memory
    void* um_ptr = um_allocator.allocate(size);

    // Allocate host buffer for verification
    unsigned char* host_ptr = static_cast<unsigned char*>(host_allocator.allocate(size));

    // Fill managed memory with test data
    std::memset(um_ptr, 0x99, size);

    // Set coarse grain access pattern (CUDA 8.0+)
    EXPECT_NO_THROW(umpire::set_coarse_grain(um_ptr, 0, size));

    // Copy to host memory to verify data integrity
    std::memcpy(host_ptr, um_ptr, size);

    // Verify data is intact after advise
    for (std::size_t i = 0; i < size; ++i) {
      ASSERT_EQ(host_ptr[i], 0x99) << "CUDA set_coarse_grain corrupted data at byte " << i;
    }

    // Test unsetting coarse grain
    EXPECT_NO_THROW(umpire::unset_coarse_grain(um_ptr, 0, size));

    // Cleanup
    um_allocator.deallocate(um_ptr);
    host_allocator.deallocate(host_ptr);
  } catch (const std::runtime_error& e) {
    // If UM allocator is not available, skip this test
    GTEST_SKIP() << "Unified Memory not available: " << e.what();
  }
}
#endif // CUDA_VERSION >= 8000
#endif // UMPIRE_ENABLE_CUDA

#if defined(UMPIRE_ENABLE_HIP)
TEST(Advise, HipSetAccessedBy)
{
  constexpr std::size_t size = 1024;

  auto& rm = umpire::ResourceManager::getInstance();

  // Check if we have managed memory support
  try {
    auto um_allocator = rm.getAllocator("UM");
    auto host_allocator = rm.getAllocator("HOST");

    // Allocate managed memory
    void* um_ptr = um_allocator.allocate(size);

    // Allocate host buffer for verification
    unsigned char* host_ptr = static_cast<unsigned char*>(host_allocator.allocate(size));

    // Fill managed memory with test data
    std::memset(um_ptr, 0xAA, size);

    // Set accessed by device 0 (should not crash or corrupt data)
    EXPECT_NO_THROW(umpire::set_accessed_by(um_ptr, 0, size));

    // Copy to host memory to verify data integrity
    std::memcpy(host_ptr, um_ptr, size);

    // Verify data is intact after advise
    for (std::size_t i = 0; i < size; ++i) {
      ASSERT_EQ(host_ptr[i], 0xAA) << "HIP set_accessed_by corrupted data at byte " << i;
    }

    // Test unsetting
    EXPECT_NO_THROW(umpire::unset_accessed_by(um_ptr, 0, size));

    // Cleanup
    um_allocator.deallocate(um_ptr);
    host_allocator.deallocate(host_ptr);
  } catch (const std::runtime_error& e) {
    // If UM allocator is not available, skip this test
    GTEST_SKIP() << "Unified Memory not available: " << e.what();
  }
}

TEST(Advise, HipSetPreferredLocation)
{
  constexpr std::size_t size = 1024;

  auto& rm = umpire::ResourceManager::getInstance();

  // Check if we have managed memory support
  try {
    auto um_allocator = rm.getAllocator("UM");
    auto host_allocator = rm.getAllocator("HOST");

    // Allocate managed memory
    void* um_ptr = um_allocator.allocate(size);

    // Allocate host buffer for verification
    unsigned char* host_ptr = static_cast<unsigned char*>(host_allocator.allocate(size));

    // Fill managed memory with test data
    std::memset(um_ptr, 0xBB, size);

    // Set preferred location to device 0
    EXPECT_NO_THROW(umpire::set_preferred_location(um_ptr, 0, size));

    // Copy to host memory to verify data integrity
    std::memcpy(host_ptr, um_ptr, size);

    // Verify data is intact after advise
    for (std::size_t i = 0; i < size; ++i) {
      ASSERT_EQ(host_ptr[i], 0xBB) << "HIP set_preferred_location corrupted data at byte " << i;
    }

    // Test unsetting
    EXPECT_NO_THROW(umpire::unset_preferred_location(um_ptr, 0, size));

    // Cleanup
    um_allocator.deallocate(um_ptr);
    host_allocator.deallocate(host_ptr);
  } catch (const std::runtime_error& e) {
    // If UM allocator is not available, skip this test
    GTEST_SKIP() << "Unified Memory not available: " << e.what();
  }
}

TEST(Advise, ExplicitHipAdvise)
{
  constexpr std::size_t size = 1024;

  auto& rm = umpire::ResourceManager::getInstance();

  // Check if we have managed memory support
  try {
    auto um_allocator = rm.getAllocator("UM");
    auto host_allocator = rm.getAllocator("HOST");

    // Allocate managed memory
    void* um_ptr = um_allocator.allocate(size);

    // Allocate host buffer for verification
    unsigned char* host_ptr = static_cast<unsigned char*>(host_allocator.allocate(size));

    // Fill managed memory with test data
    std::memset(um_ptr, 0xCC, size);

    // Explicit HIP advise operations
    EXPECT_NO_THROW(umpire::set_accessed_by<umpire::hip>(um_ptr, 0, size));
    EXPECT_NO_THROW(umpire::set_preferred_location<umpire::hip>(um_ptr, 0, size));
    EXPECT_NO_THROW(umpire::set_read_mostly<umpire::hip>(um_ptr, 0, size));

    // Copy to host memory to verify data integrity
    std::memcpy(host_ptr, um_ptr, size);

    // Verify data is intact after advise
    for (std::size_t i = 0; i < size; ++i) {
      ASSERT_EQ(host_ptr[i], 0xCC) << "Explicit HIP advise corrupted data at byte " << i;
    }

    // Cleanup
    um_allocator.deallocate(um_ptr);
    host_allocator.deallocate(host_ptr);
  } catch (const std::runtime_error& e) {
    // If UM allocator is not available, skip this test
    GTEST_SKIP() << "Unified Memory not available: " << e.what();
  }
}

#if defined(HIP_VERSION_MAJOR) && HIP_VERSION_MAJOR >= 5
TEST(Advise, HipCoarseGrain)
{
  constexpr std::size_t size = 1024;

  auto& rm = umpire::ResourceManager::getInstance();

  // Check if we have managed memory support
  try {
    auto um_allocator = rm.getAllocator("UM");
    auto host_allocator = rm.getAllocator("HOST");

    // Allocate managed memory
    void* um_ptr = um_allocator.allocate(size);

    // Allocate host buffer for verification
    unsigned char* host_ptr = static_cast<unsigned char*>(host_allocator.allocate(size));

    // Fill managed memory with test data
    std::memset(um_ptr, 0xDD, size);

    // Set coarse grain access pattern (HIP 5.0+)
    EXPECT_NO_THROW(umpire::set_coarse_grain(um_ptr, 0, size));

    // Copy to host memory to verify data integrity
    std::memcpy(host_ptr, um_ptr, size);

    // Verify data is intact after advise
    for (std::size_t i = 0; i < size; ++i) {
      ASSERT_EQ(host_ptr[i], 0xDD) << "HIP set_coarse_grain corrupted data at byte " << i;
    }

    // Test unsetting coarse grain
    EXPECT_NO_THROW(umpire::unset_coarse_grain(um_ptr, 0, size));

    // Cleanup
    um_allocator.deallocate(um_ptr);
    host_allocator.deallocate(host_ptr);
  } catch (const std::runtime_error& e) {
    // If UM allocator is not available, skip this test
    GTEST_SKIP() << "Unified Memory not available: " << e.what();
  }
}
#endif // HIP_VERSION_MAJOR >= 5
#endif // UMPIRE_ENABLE_HIP

//------------------------------------------------------------------------------
// Platform-by-value overload tests
//------------------------------------------------------------------------------

TEST(Advise, ExplicitPlatformHostThrows)
{
  constexpr std::size_t size = 512;

  auto& rm = umpire::ResourceManager::getInstance();
  auto allocator = rm.getAllocator("HOST");

  void* ptr = allocator.allocate(size);

  // Fill with test data
  std::memset(ptr, 0x22, size);

  // Advise operations should throw for host platform
  EXPECT_THROW(umpire::set_accessed_by(camp::resources::Platform::host, ptr, 0, size), std::runtime_error);
  EXPECT_THROW(umpire::set_preferred_location(camp::resources::Platform::host, ptr, 0, size), std::runtime_error);
  EXPECT_THROW(umpire::set_read_mostly(camp::resources::Platform::host, ptr, 0, size), std::runtime_error);
  EXPECT_THROW(umpire::unset_accessed_by(camp::resources::Platform::host, ptr, 0, size), std::runtime_error);
  EXPECT_THROW(umpire::unset_preferred_location(camp::resources::Platform::host, ptr, 0, size), std::runtime_error);
  EXPECT_THROW(umpire::unset_read_mostly(camp::resources::Platform::host, ptr, 0, size), std::runtime_error);

  allocator.deallocate(ptr);
}

#if defined(UMPIRE_ENABLE_CUDA)
TEST(Advise, ExplicitPlatformCudaSetAccessedBy)
{
  constexpr std::size_t size = 512;

  auto& rm = umpire::ResourceManager::getInstance();

  try {
    auto um_allocator = rm.getAllocator("UM");
    auto host_allocator = rm.getAllocator("HOST");

    void* um_ptr = um_allocator.allocate(size);
    unsigned char* host_ptr = static_cast<unsigned char*>(host_allocator.allocate(size));

    // Fill with test data
    std::memset(um_ptr, 0x33, size);

    // Set accessed by with explicit platform
    EXPECT_NO_THROW(umpire::set_accessed_by(camp::resources::Platform::cuda, um_ptr, 0, size));

    // Copy to verify
    std::memcpy(host_ptr, um_ptr, size);

    // Verify data is intact
    for (std::size_t i = 0; i < size; ++i) {
      ASSERT_EQ(host_ptr[i], 0x33) << "Data corrupted at byte " << i;
    }

    // Unset with explicit platform
    EXPECT_NO_THROW(umpire::unset_accessed_by(camp::resources::Platform::cuda, um_ptr, 0, size));

    um_allocator.deallocate(um_ptr);
    host_allocator.deallocate(host_ptr);
  } catch (const std::runtime_error& e) {
    GTEST_SKIP() << "Unified Memory not available: " << e.what();
  }
}

TEST(Advise, ExplicitPlatformCudaSetPreferredLocation)
{
  constexpr std::size_t size = 512;

  auto& rm = umpire::ResourceManager::getInstance();

  try {
    auto um_allocator = rm.getAllocator("UM");
    auto host_allocator = rm.getAllocator("HOST");

    void* um_ptr = um_allocator.allocate(size);
    unsigned char* host_ptr = static_cast<unsigned char*>(host_allocator.allocate(size));

    // Fill with test data
    std::memset(um_ptr, 0x44, size);

    // Set preferred location with explicit platform
    EXPECT_NO_THROW(umpire::set_preferred_location(camp::resources::Platform::cuda, um_ptr, 0, size));

    // Copy to verify
    std::memcpy(host_ptr, um_ptr, size);

    // Verify data is intact
    for (std::size_t i = 0; i < size; ++i) {
      ASSERT_EQ(host_ptr[i], 0x44) << "Data corrupted at byte " << i;
    }

    // Unset with explicit platform
    EXPECT_NO_THROW(umpire::unset_preferred_location(camp::resources::Platform::cuda, um_ptr, 0, size));

    um_allocator.deallocate(um_ptr);
    host_allocator.deallocate(host_ptr);
  } catch (const std::runtime_error& e) {
    GTEST_SKIP() << "Unified Memory not available: " << e.what();
  }
}

TEST(Advise, ExplicitPlatformCudaSetReadMostly)
{
  constexpr std::size_t size = 512;

  auto& rm = umpire::ResourceManager::getInstance();

  try {
    auto um_allocator = rm.getAllocator("UM");
    auto host_allocator = rm.getAllocator("HOST");

    void* um_ptr = um_allocator.allocate(size);
    unsigned char* host_ptr = static_cast<unsigned char*>(host_allocator.allocate(size));

    // Fill with test data
    std::memset(um_ptr, 0x55, size);

    // Set read mostly with explicit platform
    EXPECT_NO_THROW(umpire::set_read_mostly(camp::resources::Platform::cuda, um_ptr, 0, size));

    // Copy to verify
    std::memcpy(host_ptr, um_ptr, size);

    // Verify data is intact
    for (std::size_t i = 0; i < size; ++i) {
      ASSERT_EQ(host_ptr[i], 0x55) << "Data corrupted at byte " << i;
    }

    // Unset with explicit platform
    EXPECT_NO_THROW(umpire::unset_read_mostly(camp::resources::Platform::cuda, um_ptr, 0, size));

    um_allocator.deallocate(um_ptr);
    host_allocator.deallocate(host_ptr);
  } catch (const std::runtime_error& e) {
    GTEST_SKIP() << "Unified Memory not available: " << e.what();
  }
}
#endif // UMPIRE_ENABLE_CUDA
