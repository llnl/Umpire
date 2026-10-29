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

TEST(Copy, HostToHost)
{
  constexpr std::size_t size = 1024;
  constexpr int num_elements = size / sizeof(int);

  auto& rm = umpire::ResourceManager::getInstance();
  auto allocator = rm.getAllocator("HOST");

  // Allocate source and destination buffers
  int* source_ptr = static_cast<int*>(allocator.allocate(size));
  int* dest_ptr = static_cast<int*>(allocator.allocate(size));

  // Fill source with random data
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<int> distribution(1, 100);

  for (int i = 0; i < num_elements; ++i) {
    source_ptr[i] = distribution(gen);
  }

  // Set destination to zero
  std::memset(dest_ptr, 0, size);

  // Copy data using umpire::copy
  umpire::copy(source_ptr, dest_ptr, num_elements);

  // Verify the copy was successful
  for (int i = 0; i < num_elements; ++i) {
    ASSERT_EQ(source_ptr[i], dest_ptr[i]) << "Data mismatch at index " << i;
  }

  // Cleanup
  allocator.deallocate(source_ptr);
  allocator.deallocate(dest_ptr);
}

TEST(Copy, TypedHostToHost)
{
  constexpr std::size_t num_elements = 1024;

  auto& rm = umpire::ResourceManager::getInstance();
  auto allocator = rm.getAllocator("HOST");

  // Allocate source and destination buffers
  float* source_ptr = static_cast<float*>(allocator.allocate(num_elements * sizeof(float)));
  float* dest_ptr = static_cast<float*>(allocator.allocate(num_elements * sizeof(float)));

  // Fill source with test data (index as float)
  for (std::size_t i = 0; i < num_elements; ++i) {
    source_ptr[i] = static_cast<float>(i) + 0.5f;
  }

  // Set destination to zero
  std::memset(dest_ptr, 0, num_elements * sizeof(float));

  // Copy data using umpire::copy
  umpire::copy(source_ptr, dest_ptr, num_elements);

  // Verify the copy was successful
  for (std::size_t i = 0; i < num_elements; ++i) {
    ASSERT_FLOAT_EQ(source_ptr[i], dest_ptr[i]) << "Data mismatch at index " << i;
  }

  // Cleanup
  allocator.deallocate(source_ptr);
  allocator.deallocate(dest_ptr);
}

TEST(Copy, PartialCopy)
{
  constexpr std::size_t size = 1024;
  constexpr std::size_t partial_size = 512;
  constexpr int num_elements = size / sizeof(int);
  constexpr int partial_elements = partial_size / sizeof(int);

  auto& rm = umpire::ResourceManager::getInstance();
  auto allocator = rm.getAllocator("HOST");

  // Allocate source and destination buffers
  int* source_ptr = static_cast<int*>(allocator.allocate(size));
  int* dest_ptr = static_cast<int*>(allocator.allocate(size));

  // Fill source and destination with known patterns
  for (int i = 0; i < num_elements; ++i) {
    source_ptr[i] = i + 100; // Source pattern
    dest_ptr[i] = i;         // Destination pattern
  }

  // Copy partial data using umpire::copy
  umpire::copy(source_ptr, dest_ptr, partial_elements);

  // Verify partial copy was successful
  for (int i = 0; i < partial_elements; ++i) {
    ASSERT_EQ(source_ptr[i], dest_ptr[i]) << "Data mismatch at index " << i;
  }

  // Verify the rest of destination is unchanged
  for (int i = partial_elements; i < num_elements; ++i) {
    ASSERT_EQ(dest_ptr[i], i) << "Data unexpectedly changed at index " << i;
  }

  // Cleanup
  allocator.deallocate(source_ptr);
  allocator.deallocate(dest_ptr);
}

// Test for zero-sized copy
TEST(Copy, ZeroSize)
{
  auto& rm = umpire::ResourceManager::getInstance();
  auto allocator = rm.getAllocator("HOST");

  // Allocate source and destination buffers
  void* source_ptr = allocator.allocate(1024);
  void* dest_ptr = allocator.allocate(1024);

  // Fill with known values
  std::memset(source_ptr, 0xAA, 1024);
  std::memset(dest_ptr, 0xBB, 1024);

  // Copy with zero size - should be a no-op
  umpire::copy(source_ptr, dest_ptr, 0);

  // Check first byte to make sure it wasn't changed
  ASSERT_EQ(*static_cast<unsigned char*>(dest_ptr), 0xBB);

  // Cleanup
  allocator.deallocate(source_ptr);
  allocator.deallocate(dest_ptr);
}

// Test to validate that typed copy uses element counts, not bytes
TEST(Copy, TypedCopySemantics)
{
  constexpr std::size_t num_elements = 100;

  auto& rm = umpire::ResourceManager::getInstance();
  auto allocator = rm.getAllocator("HOST");

  // Allocate buffers for int*
  int* source = static_cast<int*>(allocator.allocate(num_elements * sizeof(int)));
  int* dest = static_cast<int*>(allocator.allocate(num_elements * sizeof(int)));

  // Fill source with known pattern
  for (std::size_t i = 0; i < num_elements; ++i) {
    source[i] = static_cast<int>(i * 7 + 13);
  }

  // Zero out destination
  std::memset(dest, 0, num_elements * sizeof(int));

  // Copy using element count (not bytes)
  umpire::copy(source, dest, num_elements);

  // Verify exactly num_elements were copied
  for (std::size_t i = 0; i < num_elements; ++i) {
    ASSERT_EQ(dest[i], static_cast<int>(i * 7 + 13)) << "Element " << i << " not copied correctly";
  }

  // Cleanup
  allocator.deallocate(source);
  allocator.deallocate(dest);
}

// Test to validate void* copy uses bytes, not elements
TEST(Copy, VoidVsTypedSemantics)
{
  constexpr std::size_t byte_count = 100;

  auto& rm = umpire::ResourceManager::getInstance();
  auto allocator = rm.getAllocator("HOST");

  // Allocate as void*
  void* void_src = allocator.allocate(byte_count);
  void* void_dst = allocator.allocate(byte_count);

  // Fill with byte pattern
  std::memset(void_src, 0xAB, byte_count);
  std::memset(void_dst, 0xCD, byte_count);

  // Copy exact byte count
  umpire::copy(void_src, void_dst, byte_count);

  // Verify byte-for-byte copy
  unsigned char* src_bytes = static_cast<unsigned char*>(void_src);
  unsigned char* dst_bytes = static_cast<unsigned char*>(void_dst);
  for (std::size_t i = 0; i < byte_count; ++i) {
    ASSERT_EQ(dst_bytes[i], src_bytes[i]) << "Byte " << i << " not copied correctly";
  }

  // Cleanup
  allocator.deallocate(void_src);
  allocator.deallocate(void_dst);
}

TEST(Copy, HostToHostAsync)
{
  constexpr std::size_t size = 1024;
  constexpr int num_elements = size / sizeof(int);

  auto& rm = umpire::ResourceManager::getInstance();
  auto allocator = rm.getAllocator("HOST");

  // Allocate source and destination buffers
  int* source_ptr = static_cast<int*>(allocator.allocate(size));
  int* dest_ptr = static_cast<int*>(allocator.allocate(size));

  // Fill source with test data
  for (int i = 0; i < num_elements; ++i) {
    source_ptr[i] = i + 500;
  }

  // Set destination to zero
  std::memset(dest_ptr, 0, size);

  // Create host resource for async operation
  camp::resources::Resource host_ctx{camp::resources::Host{}};

  // Copy data using async variant
  auto event = umpire::copy(source_ptr, dest_ptr, num_elements, host_ctx);

  // Wait for async operation to complete
  static_cast<camp::resources::Event>(event).wait();

  // Verify the copy was successful
  for (int i = 0; i < num_elements; ++i) {
    ASSERT_EQ(source_ptr[i], dest_ptr[i]) << "Async copy failed at index " << i;
  }

  // Cleanup
  allocator.deallocate(source_ptr);
  allocator.deallocate(dest_ptr);
}

#if defined(UMPIRE_ENABLE_CUDA)
TEST(Copy, CudaToHostAsync)
{
  constexpr std::size_t size = 1024;
  constexpr int num_elements = size / sizeof(int);

  auto& rm = umpire::ResourceManager::getInstance();
  auto cuda_allocator = rm.getAllocator("DEVICE");
  auto host_allocator = rm.getAllocator("HOST");

  // Allocate device and host buffers
  int* device_ptr = static_cast<int*>(cuda_allocator.allocate(size));
  int* host_ptr = static_cast<int*>(host_allocator.allocate(size));
  int* temp_host = static_cast<int*>(host_allocator.allocate(size));

  // Fill temp host buffer with test data
  for (int i = 0; i < num_elements; ++i) {
    temp_host[i] = i + 600;
  }

  // Copy data to device synchronously
  umpire::copy(temp_host, device_ptr, num_elements);

  // Zero out destination
  std::memset(host_ptr, 0, size);

  // Create CUDA resource for async operation
  camp::resources::Resource cuda_ctx{camp::resources::Cuda{}};

  // Async copy from device to host
  auto event = umpire::copy(device_ptr, host_ptr, num_elements, cuda_ctx);

  // Wait for async operation to complete
  static_cast<camp::resources::Event>(event).wait();

  // Verify the copy was successful
  for (int i = 0; i < num_elements; ++i) {
    ASSERT_EQ(host_ptr[i], i + 600) << "Async CUDA->Host copy failed at index " << i;
  }

  // Cleanup
  cuda_allocator.deallocate(device_ptr);
  host_allocator.deallocate(host_ptr);
  host_allocator.deallocate(temp_host);
}

TEST(Copy, HostToCudaAsync)
{
  constexpr std::size_t size = 1024;
  constexpr int num_elements = size / sizeof(int);

  auto& rm = umpire::ResourceManager::getInstance();
  auto cuda_allocator = rm.getAllocator("DEVICE");
  auto host_allocator = rm.getAllocator("HOST");

  // Allocate device and host buffers
  int* device_ptr = static_cast<int*>(cuda_allocator.allocate(size));
  int* host_src = static_cast<int*>(host_allocator.allocate(size));
  int* host_verify = static_cast<int*>(host_allocator.allocate(size));

  // Fill source with test data
  for (int i = 0; i < num_elements; ++i) {
    host_src[i] = i + 700;
  }

  // Create CUDA resource for async operation
  camp::resources::Resource cuda_ctx{camp::resources::Cuda{}};

  // Async copy from host to device
  auto event = umpire::copy(host_src, device_ptr, num_elements, cuda_ctx);

  // Wait for async operation to complete
  static_cast<camp::resources::Event>(event).wait();

  // Copy back to verify
  umpire::copy(device_ptr, host_verify, num_elements);

  // Verify the copy was successful
  for (int i = 0; i < num_elements; ++i) {
    ASSERT_EQ(host_verify[i], i + 700) << "Async Host->CUDA copy failed at index " << i;
  }

  // Cleanup
  cuda_allocator.deallocate(device_ptr);
  host_allocator.deallocate(host_src);
  host_allocator.deallocate(host_verify);
}
#endif

//------------------------------------------------------------------------------
// Platform-by-value overload tests
//------------------------------------------------------------------------------

TEST(Copy, ExplicitPlatformHostToHost)
{
  constexpr std::size_t num_elements = 256;

  auto& rm = umpire::ResourceManager::getInstance();
  auto allocator = rm.getAllocator("HOST");

  int* source_ptr = static_cast<int*>(allocator.allocate(num_elements * sizeof(int)));
  int* dest_ptr = static_cast<int*>(allocator.allocate(num_elements * sizeof(int)));

  // Initialize source data
  for (std::size_t i = 0; i < num_elements; ++i) {
    source_ptr[i] = static_cast<int>(i * 3 + 42);
  }

  // Zero destination
  std::memset(dest_ptr, 0, num_elements * sizeof(int));

  // Copy using explicit platform parameters
  umpire::copy(camp::resources::Platform::host, camp::resources::Platform::host, source_ptr, dest_ptr, num_elements);

  // Verify
  for (std::size_t i = 0; i < num_elements; ++i) {
    ASSERT_EQ(source_ptr[i], dest_ptr[i]) << "Data mismatch at index " << i;
  }

  allocator.deallocate(source_ptr);
  allocator.deallocate(dest_ptr);
}

TEST(Copy, ExplicitPlatformHostToHostAsync)
{
  constexpr std::size_t num_elements = 256;

  auto& rm = umpire::ResourceManager::getInstance();
  auto allocator = rm.getAllocator("HOST");

  int* source_ptr = static_cast<int*>(allocator.allocate(num_elements * sizeof(int)));
  int* dest_ptr = static_cast<int*>(allocator.allocate(num_elements * sizeof(int)));

  // Initialize source data
  for (std::size_t i = 0; i < num_elements; ++i) {
    source_ptr[i] = static_cast<int>(i * 5 + 17);
  }

  // Zero destination
  std::memset(dest_ptr, 0, num_elements * sizeof(int));

  // Create host resource
  camp::resources::Resource host_ctx{camp::resources::Host{}};

  // Async copy using explicit platform parameters
  auto event = umpire::copy(camp::resources::Platform::host, camp::resources::Platform::host, source_ptr, dest_ptr,
                            num_elements, host_ctx);

  // Wait for completion
  static_cast<camp::resources::Event>(event).wait();

  // Verify
  for (std::size_t i = 0; i < num_elements; ++i) {
    ASSERT_EQ(source_ptr[i], dest_ptr[i]) << "Data mismatch at index " << i;
  }

  allocator.deallocate(source_ptr);
  allocator.deallocate(dest_ptr);
}

#if defined(UMPIRE_ENABLE_CUDA)
TEST(Copy, ExplicitPlatformCudaToHost)
{
  constexpr std::size_t num_elements = 256;

  auto& rm = umpire::ResourceManager::getInstance();
  auto cuda_allocator = rm.getAllocator("DEVICE");
  auto host_allocator = rm.getAllocator("HOST");

  int* device_ptr = static_cast<int*>(cuda_allocator.allocate(num_elements * sizeof(int)));
  int* host_src = static_cast<int*>(host_allocator.allocate(num_elements * sizeof(int)));
  int* host_dst = static_cast<int*>(host_allocator.allocate(num_elements * sizeof(int)));

  // Initialize host source
  for (std::size_t i = 0; i < num_elements; ++i) {
    host_src[i] = static_cast<int>(i * 7 + 99);
  }

  // Copy to device first
  umpire::copy(host_src, device_ptr, num_elements);

  // Zero destination
  std::memset(host_dst, 0, num_elements * sizeof(int));

  // Copy from device to host using explicit platforms
  umpire::copy(camp::resources::Platform::cuda, camp::resources::Platform::host, device_ptr, host_dst, num_elements);

  // Verify
  for (std::size_t i = 0; i < num_elements; ++i) {
    ASSERT_EQ(host_dst[i], static_cast<int>(i * 7 + 99)) << "Data mismatch at index " << i;
  }

  cuda_allocator.deallocate(device_ptr);
  host_allocator.deallocate(host_src);
  host_allocator.deallocate(host_dst);
}

TEST(Copy, ExplicitPlatformHostToCuda)
{
  constexpr std::size_t num_elements = 256;

  auto& rm = umpire::ResourceManager::getInstance();
  auto cuda_allocator = rm.getAllocator("DEVICE");
  auto host_allocator = rm.getAllocator("HOST");

  int* device_ptr = static_cast<int*>(cuda_allocator.allocate(num_elements * sizeof(int)));
  int* host_src = static_cast<int*>(host_allocator.allocate(num_elements * sizeof(int)));
  int* host_verify = static_cast<int*>(host_allocator.allocate(num_elements * sizeof(int)));

  // Initialize host source
  for (std::size_t i = 0; i < num_elements; ++i) {
    host_src[i] = static_cast<int>(i * 11 + 13);
  }

  // Copy from host to device using explicit platforms
  umpire::copy(camp::resources::Platform::host, camp::resources::Platform::cuda, host_src, device_ptr, num_elements);

  // Copy back to verify
  umpire::copy(device_ptr, host_verify, num_elements);

  // Verify
  for (std::size_t i = 0; i < num_elements; ++i) {
    ASSERT_EQ(host_verify[i], static_cast<int>(i * 11 + 13)) << "Data mismatch at index " << i;
  }

  cuda_allocator.deallocate(device_ptr);
  host_allocator.deallocate(host_src);
  host_allocator.deallocate(host_verify);
}

TEST(Copy, ExplicitPlatformCudaToHostAsync)
{
  constexpr std::size_t num_elements = 256;

  auto& rm = umpire::ResourceManager::getInstance();
  auto cuda_allocator = rm.getAllocator("DEVICE");
  auto host_allocator = rm.getAllocator("HOST");

  int* device_ptr = static_cast<int*>(cuda_allocator.allocate(num_elements * sizeof(int)));
  int* host_src = static_cast<int*>(host_allocator.allocate(num_elements * sizeof(int)));
  int* host_dst = static_cast<int*>(host_allocator.allocate(num_elements * sizeof(int)));

  // Initialize and copy to device
  for (std::size_t i = 0; i < num_elements; ++i) {
    host_src[i] = static_cast<int>(i * 13 + 79);
  }
  umpire::copy(host_src, device_ptr, num_elements);

  // Zero destination
  std::memset(host_dst, 0, num_elements * sizeof(int));

  // Create CUDA resource
  camp::resources::Resource cuda_ctx{camp::resources::Cuda{}};

  // Async copy using explicit platforms
  auto event = umpire::copy(camp::resources::Platform::cuda, camp::resources::Platform::host, device_ptr, host_dst,
                            num_elements, cuda_ctx);

  // Wait for completion
  static_cast<camp::resources::Event>(event).wait();

  // Verify
  for (std::size_t i = 0; i < num_elements; ++i) {
    ASSERT_EQ(host_dst[i], static_cast<int>(i * 13 + 79)) << "Data mismatch at index " << i;
  }

  cuda_allocator.deallocate(device_ptr);
  host_allocator.deallocate(host_src);
  host_allocator.deallocate(host_dst);
}
#endif
