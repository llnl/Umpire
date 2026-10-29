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

TEST(Memset, HostMemsetBasic)
{
  constexpr std::size_t size = 1024;
  constexpr int value = 0x42;

  auto& rm = umpire::ResourceManager::getInstance();
  auto allocator = rm.getAllocator("HOST");

  // Allocate buffer
  void* ptr = allocator.allocate(size);

  // Fill with different pattern first
  std::memset(ptr, 0xAA, size);

  // Use umpire::memset to set to value
  umpire::memset(ptr, value, size);

  // Verify the memset was successful
  unsigned char* byte_ptr = static_cast<unsigned char*>(ptr);
  for (std::size_t i = 0; i < size; ++i) {
    ASSERT_EQ(byte_ptr[i], value) << "Memset failed at byte " << i;
  }

  // Cleanup
  allocator.deallocate(ptr);
}

TEST(Memset, TypedMemsetInt)
{
  constexpr std::size_t num_elements = 256;
  constexpr int value = 0;

  auto& rm = umpire::ResourceManager::getInstance();
  auto allocator = rm.getAllocator("HOST");

  // Allocate buffer for integers
  int* ptr = static_cast<int*>(allocator.allocate(num_elements * sizeof(int)));

  // Fill with pattern first
  for (std::size_t i = 0; i < num_elements; ++i) {
    ptr[i] = static_cast<int>(i);
  }

  // Use umpire::memset to zero out (pass element count for typed pointer)
  umpire::memset(ptr, value, num_elements);

  // Verify each int is zero
  for (std::size_t i = 0; i < num_elements; ++i) {
    ASSERT_EQ(ptr[i], 0) << "Memset failed at element " << i;
  }

  // Cleanup
  allocator.deallocate(ptr);
}

TEST(Memset, PartialMemset)
{
  constexpr std::size_t total_size = 1024;
  constexpr std::size_t partial_size = 512;
  constexpr int set_value = 0x55;
  constexpr int initial_value = 0xAA;

  auto& rm = umpire::ResourceManager::getInstance();
  auto allocator = rm.getAllocator("HOST");

  // Allocate buffer
  unsigned char* ptr = static_cast<unsigned char*>(allocator.allocate(total_size));

  // Fill entire buffer with initial pattern
  std::memset(ptr, initial_value, total_size);

  // Use umpire::memset on first half only
  umpire::memset(ptr, set_value, partial_size);

  // Verify first half is set to new value
  for (std::size_t i = 0; i < partial_size; ++i) {
    ASSERT_EQ(ptr[i], set_value) << "Memset failed at byte " << i;
  }

  // Verify second half is unchanged
  for (std::size_t i = partial_size; i < total_size; ++i) {
    ASSERT_EQ(ptr[i], initial_value) << "Unchanged region modified at byte " << i;
  }

  // Cleanup
  allocator.deallocate(ptr);
}

TEST(Memset, ZeroSize)
{
  constexpr std::size_t size = 1024;
  constexpr int initial_value = 0xCC;
  constexpr int memset_value = 0x33;

  auto& rm = umpire::ResourceManager::getInstance();
  auto allocator = rm.getAllocator("HOST");

  // Allocate buffer
  unsigned char* ptr = static_cast<unsigned char*>(allocator.allocate(size));

  // Fill with initial pattern
  std::memset(ptr, initial_value, size);

  // Use umpire::memset with zero size - should be no-op
  umpire::memset(ptr, memset_value, 0);

  // Verify buffer is unchanged
  for (std::size_t i = 0; i < size; ++i) {
    ASSERT_EQ(ptr[i], initial_value) << "Zero-size memset modified byte " << i;
  }

  // Cleanup
  allocator.deallocate(ptr);
}

TEST(Memset, DifferentValues)
{
  constexpr std::size_t size = 256;
  constexpr unsigned char test_values[] = {0x00, 0x01, 0x7F, 0x80, 0xFE, 0xFF};

  auto& rm = umpire::ResourceManager::getInstance();
  auto allocator = rm.getAllocator("HOST");

  // Allocate buffer
  unsigned char* ptr = static_cast<unsigned char*>(allocator.allocate(size));

  // Test each value
  for (unsigned char test_value : test_values) {
    // Use umpire::memset with test value
    umpire::memset(ptr, test_value, size);

    // Verify all bytes are set to test value
    for (std::size_t i = 0; i < size; ++i) {
      ASSERT_EQ(ptr[i], test_value) << "Memset failed for value 0x" << std::hex << static_cast<int>(test_value)
                                    << " at byte " << std::dec << i;
    }
  }

  // Cleanup
  allocator.deallocate(ptr);
}

#if defined(UMPIRE_ENABLE_CUDA)
TEST(Memset, CudaMemset)
{
  constexpr std::size_t size = 1024;
  constexpr int value = 0x42;

  auto& rm = umpire::ResourceManager::getInstance();
  auto cuda_allocator = rm.getAllocator("DEVICE");
  auto host_allocator = rm.getAllocator("HOST");

  // Allocate device buffer
  unsigned char* device_ptr = static_cast<unsigned char*>(cuda_allocator.allocate(size));

  // Allocate host buffer for verification
  unsigned char* host_ptr = static_cast<unsigned char*>(host_allocator.allocate(size));

  // Use umpire::memset on device memory
  umpire::memset(device_ptr, value, size);

  // Copy back to host for verification
  umpire::copy(device_ptr, host_ptr, size);

  // Verify the memset was successful
  for (std::size_t i = 0; i < size; ++i) {
    ASSERT_EQ(host_ptr[i], value) << "CUDA memset failed at byte " << i;
  }

  // Cleanup
  cuda_allocator.deallocate(device_ptr);
  host_allocator.deallocate(host_ptr);
}

TEST(Memset, ExplicitCudaMemset)
{
  constexpr std::size_t size = 1024;
  constexpr int value = 0x55;

  auto& rm = umpire::ResourceManager::getInstance();
  auto cuda_allocator = rm.getAllocator("DEVICE");
  auto host_allocator = rm.getAllocator("HOST");

  // Allocate device buffer
  unsigned char* device_ptr = static_cast<unsigned char*>(cuda_allocator.allocate(size));

  // Allocate host buffer for verification
  unsigned char* host_ptr = static_cast<unsigned char*>(host_allocator.allocate(size));

  // Use explicit platform memset
  umpire::memset<umpire::cuda>(device_ptr, value, size);

  // Copy back to host for verification
  umpire::copy(device_ptr, host_ptr, size);

  // Verify the memset was successful
  for (std::size_t i = 0; i < size; ++i) {
    ASSERT_EQ(host_ptr[i], value) << "Explicit CUDA memset failed at byte " << i;
  }

  // Cleanup
  cuda_allocator.deallocate(device_ptr);
  host_allocator.deallocate(host_ptr);
}
#endif // UMPIRE_ENABLE_CUDA

#if defined(UMPIRE_ENABLE_HIP)
TEST(Memset, HipMemset)
{
  constexpr std::size_t size = 1024;
  constexpr int value = 0x77;

  auto& rm = umpire::ResourceManager::getInstance();
  auto hip_allocator = rm.getAllocator("DEVICE");
  auto host_allocator = rm.getAllocator("HOST");

  // Allocate device buffer
  unsigned char* device_ptr = static_cast<unsigned char*>(hip_allocator.allocate(size));

  // Allocate host buffer for verification
  unsigned char* host_ptr = static_cast<unsigned char*>(host_allocator.allocate(size));

  // Use umpire::memset on device memory
  umpire::memset(device_ptr, value, size);

  // Copy back to host for verification
  umpire::copy(device_ptr, host_ptr, size);

  // Verify the memset was successful
  for (std::size_t i = 0; i < size; ++i) {
    ASSERT_EQ(host_ptr[i], value) << "HIP memset failed at byte " << i;
  }

  // Cleanup
  hip_allocator.deallocate(device_ptr);
  host_allocator.deallocate(host_ptr);
}

TEST(Memset, ExplicitHipMemset)
{
  constexpr std::size_t size = 1024;
  constexpr int value = 0x88;

  auto& rm = umpire::ResourceManager::getInstance();
  auto hip_allocator = rm.getAllocator("DEVICE");
  auto host_allocator = rm.getAllocator("HOST");

  // Allocate device buffer
  unsigned char* device_ptr = static_cast<unsigned char*>(hip_allocator.allocate(size));

  // Allocate host buffer for verification
  unsigned char* host_ptr = static_cast<unsigned char*>(host_allocator.allocate(size));

  // Use explicit platform memset
  umpire::memset<umpire::hip>(device_ptr, value, size);

  // Copy back to host for verification
  umpire::copy(device_ptr, host_ptr, size);

  // Verify the memset was successful
  for (std::size_t i = 0; i < size; ++i) {
    ASSERT_EQ(host_ptr[i], value) << "Explicit HIP memset failed at byte " << i;
  }

  // Cleanup
  hip_allocator.deallocate(device_ptr);
  host_allocator.deallocate(host_ptr);
}
#endif // UMPIRE_ENABLE_HIP

TEST(Memset, HostMemsetAsync)
{
  constexpr std::size_t size = 1024;
  constexpr int value = 0x77;

  auto& rm = umpire::ResourceManager::getInstance();
  auto allocator = rm.getAllocator("HOST");

  // Allocate buffer
  unsigned char* ptr = static_cast<unsigned char*>(allocator.allocate(size));

  // Fill with different pattern first
  std::memset(ptr, 0xAA, size);

  // Create host resource for async operation
  camp::resources::Resource host_ctx{camp::resources::Host{}};

  // Use async memset
  auto event = umpire::memset(ptr, value, size, host_ctx);

  // Wait for async operation to complete
  static_cast<camp::resources::Event>(event).wait();

  // Verify the memset was successful
  for (std::size_t i = 0; i < size; ++i) {
    ASSERT_EQ(ptr[i], value) << "Async host memset failed at byte " << i;
  }

  // Cleanup
  allocator.deallocate(ptr);
}

#if defined(UMPIRE_ENABLE_CUDA)
TEST(Memset, CudaMemsetAsync)
{
  constexpr std::size_t size = 1024;
  constexpr int value = 0x99;

  auto& rm = umpire::ResourceManager::getInstance();
  auto cuda_allocator = rm.getAllocator("DEVICE");
  auto host_allocator = rm.getAllocator("HOST");

  // Allocate device buffer
  unsigned char* device_ptr = static_cast<unsigned char*>(cuda_allocator.allocate(size));

  // Allocate host buffer for verification
  unsigned char* host_ptr = static_cast<unsigned char*>(host_allocator.allocate(size));

  // Create CUDA resource for async operation
  camp::resources::Resource cuda_ctx{camp::resources::Cuda{}};

  // Use async memset on device memory
  auto event = umpire::memset(device_ptr, value, size, cuda_ctx);

  // Wait for async operation to complete
  static_cast<camp::resources::Event>(event).wait();

  // Copy back to host for verification
  umpire::copy(device_ptr, host_ptr, size);

  // Verify the memset was successful
  for (std::size_t i = 0; i < size; ++i) {
    ASSERT_EQ(host_ptr[i], value) << "Async CUDA memset failed at byte " << i;
  }

  // Cleanup
  cuda_allocator.deallocate(device_ptr);
  host_allocator.deallocate(host_ptr);
}
#endif

//------------------------------------------------------------------------------
// Platform-by-value overload tests
//------------------------------------------------------------------------------

TEST(Memset, ExplicitPlatformHost)
{
  constexpr std::size_t size = 512;
  constexpr int value = 0x33;

  auto& rm = umpire::ResourceManager::getInstance();
  auto allocator = rm.getAllocator("HOST");

  unsigned char* ptr = static_cast<unsigned char*>(allocator.allocate(size));

  // Fill with initial pattern
  std::memset(ptr, 0xAA, size);

  // Use explicit platform memset
  umpire::memset(camp::resources::Platform::host, ptr, value, size);

  // Verify
  for (std::size_t i = 0; i < size; ++i) {
    ASSERT_EQ(ptr[i], value) << "Memset failed at byte " << i;
  }

  allocator.deallocate(ptr);
}

TEST(Memset, ExplicitPlatformHostAsync)
{
  constexpr std::size_t size = 512;
  constexpr int value = 0x66;

  auto& rm = umpire::ResourceManager::getInstance();
  auto allocator = rm.getAllocator("HOST");

  unsigned char* ptr = static_cast<unsigned char*>(allocator.allocate(size));

  // Fill with initial pattern
  std::memset(ptr, 0xBB, size);

  // Create host resource
  camp::resources::Resource host_ctx{camp::resources::Host{}};

  // Use explicit platform async memset
  auto event = umpire::memset(camp::resources::Platform::host, ptr, value, size, host_ctx);

  // Wait for completion
  static_cast<camp::resources::Event>(event).wait();

  // Verify
  for (std::size_t i = 0; i < size; ++i) {
    ASSERT_EQ(ptr[i], value) << "Async memset failed at byte " << i;
  }

  allocator.deallocate(ptr);
}

#if defined(UMPIRE_ENABLE_CUDA)
TEST(Memset, ExplicitPlatformCuda)
{
  constexpr std::size_t size = 512;
  constexpr int value = 0x44;

  auto& rm = umpire::ResourceManager::getInstance();
  auto cuda_allocator = rm.getAllocator("DEVICE");
  auto host_allocator = rm.getAllocator("HOST");

  unsigned char* device_ptr = static_cast<unsigned char*>(cuda_allocator.allocate(size));
  unsigned char* host_ptr = static_cast<unsigned char*>(host_allocator.allocate(size));

  // Use explicit platform memset on device
  umpire::memset(camp::resources::Platform::cuda, device_ptr, value, size);

  // Copy back to verify
  umpire::copy(device_ptr, host_ptr, size);

  // Verify
  for (std::size_t i = 0; i < size; ++i) {
    ASSERT_EQ(host_ptr[i], value) << "CUDA memset failed at byte " << i;
  }

  cuda_allocator.deallocate(device_ptr);
  host_allocator.deallocate(host_ptr);
}

TEST(Memset, ExplicitPlatformCudaAsync)
{
  constexpr std::size_t size = 512;
  constexpr int value = 0xAB;

  auto& rm = umpire::ResourceManager::getInstance();
  auto cuda_allocator = rm.getAllocator("DEVICE");
  auto host_allocator = rm.getAllocator("HOST");

  unsigned char* device_ptr = static_cast<unsigned char*>(cuda_allocator.allocate(size));
  unsigned char* host_ptr = static_cast<unsigned char*>(host_allocator.allocate(size));

  // Create CUDA resource
  camp::resources::Resource cuda_ctx{camp::resources::Cuda{}};

  // Use explicit platform async memset on device
  auto event = umpire::memset(camp::resources::Platform::cuda, device_ptr, value, size, cuda_ctx);

  // Wait for completion
  static_cast<camp::resources::Event>(event).wait();

  // Copy back to verify
  umpire::copy(device_ptr, host_ptr, size);

  // Verify
  for (std::size_t i = 0; i < size; ++i) {
    ASSERT_EQ(host_ptr[i], value) << "Async CUDA memset failed at byte " << i;
  }

  cuda_allocator.deallocate(device_ptr);
  host_allocator.deallocate(host_ptr);
}
#endif