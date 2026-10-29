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

TEST(Reallocate, HostReallocateGrow)
{
  constexpr std::size_t initial_size = 512;
  constexpr std::size_t final_size = 1024;
  constexpr int num_initial_elements = initial_size / sizeof(int);
  constexpr int num_final_elements = final_size / sizeof(int);

  auto& rm = umpire::ResourceManager::getInstance();
  auto allocator = rm.getAllocator("HOST");

  // Allocate initial buffer
  int* ptr = static_cast<int*>(allocator.allocate(initial_size));

  // Fill with test data
  for (int i = 0; i < num_initial_elements; ++i) {
    ptr[i] = i + 100;
  }

  // Reallocate to larger size
  ptr = umpire::reallocate(&ptr, num_final_elements);

  // Verify original data is preserved
  for (int i = 0; i < num_initial_elements; ++i) {
    ASSERT_EQ(ptr[i], i + 100) << "Original data lost at index " << i;
  }

  // Fill new portion
  for (int i = num_initial_elements; i < num_final_elements; ++i) {
    ptr[i] = i + 200;
  }

  // Verify new data can be written
  for (int i = num_initial_elements; i < num_final_elements; ++i) {
    ASSERT_EQ(ptr[i], i + 200) << "New data write failed at index " << i;
  }

  // Cleanup
  allocator.deallocate(ptr);
}

TEST(Reallocate, HostReallocateShrink)
{
  constexpr std::size_t initial_size = 1024;
  constexpr std::size_t final_size = 512;
  constexpr int num_initial_elements = initial_size / sizeof(int);
  constexpr int num_final_elements = final_size / sizeof(int);

  auto& rm = umpire::ResourceManager::getInstance();
  auto allocator = rm.getAllocator("HOST");

  // Allocate initial buffer
  int* ptr = static_cast<int*>(allocator.allocate(initial_size));

  // Fill with test data
  for (int i = 0; i < num_initial_elements; ++i) {
    ptr[i] = i * 2;
  }

  // Reallocate to smaller size
  ptr = umpire::reallocate(&ptr, num_final_elements);

  // Verify preserved data is correct
  for (int i = 0; i < num_final_elements; ++i) {
    ASSERT_EQ(ptr[i], i * 2) << "Data corruption at index " << i << " after shrinking";
  }

  // Cleanup
  allocator.deallocate(ptr);
}

TEST(Reallocate, HostReallocateSameSize)
{
  constexpr std::size_t size = 1024;
  constexpr int num_elements = size / sizeof(int);

  auto& rm = umpire::ResourceManager::getInstance();
  auto allocator = rm.getAllocator("HOST");

  // Allocate buffer
  int* ptr = static_cast<int*>(allocator.allocate(size));

  // Fill with test data
  for (int i = 0; i < num_elements; ++i) {
    ptr[i] = i * 3 + 42;
  }

  // Reallocate to same size
  ptr = umpire::reallocate(&ptr, num_elements);

  // Verify all data is preserved
  for (int i = 0; i < num_elements; ++i) {
    ASSERT_EQ(ptr[i], i * 3 + 42) << "Data corruption at index " << i << " for same-size realloc";
  }

  // Cleanup
  allocator.deallocate(ptr);
}

TEST(Reallocate, HostReallocateToZero)
{
  constexpr std::size_t initial_size = 1024;

  auto& rm = umpire::ResourceManager::getInstance();
  auto allocator = rm.getAllocator("HOST");

  // Allocate initial buffer
  int* ptr = static_cast<int*>(allocator.allocate(initial_size));

  // Fill with test data
  *ptr = 12345;

  // Reallocate to zero size
  ptr = static_cast<int*>(umpire::reallocate(&ptr, 0));

  // Should get a valid pointer to zero-sized allocation
  ASSERT_NE(ptr, nullptr);

  // Cleanup
  allocator.deallocate(ptr);
}

TEST(Reallocate, HostReallocateFromNull)
{
  constexpr std::size_t size = 1024;
  constexpr int num_elements = size / sizeof(int);

  // Start with null pointer
  int* ptr = nullptr;

  // Reallocate from null - should work like malloc
  ptr = umpire::reallocate(&ptr, num_elements);
  ASSERT_NE(ptr, nullptr);

  // Should be able to write to the allocated memory
  for (int i = 0; i < static_cast<int>(size / sizeof(int)); ++i) {
    ptr[i] = i + 500;
  }

  // Verify data
  for (int i = 0; i < static_cast<int>(size / sizeof(int)); ++i) {
    ASSERT_EQ(ptr[i], i + 500) << "Data write failed at index " << i;
  }

  // Cleanup
  auto& rm = umpire::ResourceManager::getInstance();
  rm.deallocate(ptr);
}

TEST(Reallocate, TypedReallocate)
{
  constexpr std::size_t initial_elements = 128;
  constexpr std::size_t final_elements = 256;

  auto& rm = umpire::ResourceManager::getInstance();
  auto allocator = rm.getAllocator("HOST");

  // Allocate initial buffer for doubles
  double* ptr = static_cast<double*>(allocator.allocate(initial_elements * sizeof(double)));

  // Fill with test data
  for (std::size_t i = 0; i < initial_elements; ++i) {
    ptr[i] = static_cast<double>(i) + 0.5;
  }

  // Reallocate to larger size
  ptr = umpire::reallocate(&ptr, final_elements);

  // Verify original data is preserved
  for (std::size_t i = 0; i < initial_elements; ++i) {
    ASSERT_DOUBLE_EQ(ptr[i], static_cast<double>(i) + 0.5) << "Double data lost at index " << i;
  }

  // Cleanup
  allocator.deallocate(ptr);
}

TEST(Reallocate, VoidPointerReallocate)
{
  constexpr std::size_t initial_size = 512;
  constexpr std::size_t final_size = 1024;

  auto& rm = umpire::ResourceManager::getInstance();
  auto allocator = rm.getAllocator("HOST");

  // Allocate initial buffer as void*
  void* ptr = allocator.allocate(initial_size);

  // Fill with pattern
  std::memset(ptr, 0xAB, initial_size);

  // Reallocate using void* interface
  ptr = umpire::reallocate(&ptr, final_size);

  // Verify pattern is preserved in original portion
  unsigned char* byte_ptr = static_cast<unsigned char*>(ptr);
  for (std::size_t i = 0; i < initial_size; ++i) {
    ASSERT_EQ(byte_ptr[i], 0xAB) << "Pattern lost at byte " << i;
  }

  // Cleanup
  allocator.deallocate(ptr);
}

#if defined(UMPIRE_ENABLE_CUDA)
TEST(Reallocate, CudaReallocate)
{
  constexpr std::size_t initial_size = 512;
  constexpr std::size_t final_size = 1024;
  constexpr int num_initial_elements = initial_size / sizeof(int);
  constexpr int num_final_elements = final_size / sizeof(int);

  auto& rm = umpire::ResourceManager::getInstance();
  auto cuda_allocator = rm.getAllocator("DEVICE");
  auto host_allocator = rm.getAllocator("HOST");

  // Allocate device buffer
  int* device_ptr = static_cast<int*>(cuda_allocator.allocate(initial_size));

  // Allocate host buffer for data setup
  int* host_ptr = static_cast<int*>(host_allocator.allocate(final_size));

  // Fill host buffer with test data
  for (int i = 0; i < num_initial_elements; ++i) {
    host_ptr[i] = i + 300;
  }

  // Copy initial data to device
  umpire::copy(host_ptr, device_ptr, num_initial_elements);

  // Reallocate device memory
  device_ptr = umpire::reallocate(&device_ptr, num_final_elements);

  // Copy back to host for verification
  umpire::copy(device_ptr, host_ptr, num_final_elements);

  // Verify original data is preserved
  for (int i = 0; i < num_initial_elements; ++i) {
    ASSERT_EQ(host_ptr[i], i + 300) << "CUDA reallocate lost data at index " << i;
  }

  // Cleanup
  cuda_allocator.deallocate(device_ptr);
  host_allocator.deallocate(host_ptr);
}
#endif // UMPIRE_ENABLE_CUDA

#if defined(UMPIRE_ENABLE_HIP)
TEST(Reallocate, HipReallocate)
{
  constexpr std::size_t initial_size = 512;
  constexpr std::size_t final_size = 1024;
  constexpr int num_initial_elements = initial_size / sizeof(int);
  constexpr int num_final_elements = final_size / sizeof(int);

  auto& rm = umpire::ResourceManager::getInstance();
  auto hip_allocator = rm.getAllocator("DEVICE");
  auto host_allocator = rm.getAllocator("HOST");

  // Allocate device buffer
  int* device_ptr = static_cast<int*>(hip_allocator.allocate(initial_size));

  // Allocate host buffer for data setup
  int* host_ptr = static_cast<int*>(host_allocator.allocate(final_size));

  // Fill host buffer with test data
  for (int i = 0; i < num_initial_elements; ++i) {
    host_ptr[i] = i + 400;
  }

  // Copy initial data to device
  umpire::copy(host_ptr, device_ptr, num_initial_elements);

  // Reallocate device memory
  device_ptr = umpire::reallocate(&device_ptr, num_final_elements);

  // Copy back to host for verification
  umpire::copy(device_ptr, host_ptr, num_final_elements);

  // Verify original data is preserved
  for (int i = 0; i < num_initial_elements; ++i) {
    ASSERT_EQ(host_ptr[i], i + 400) << "HIP reallocate lost data at index " << i;
  }

  // Cleanup
  hip_allocator.deallocate(device_ptr);
  host_allocator.deallocate(host_ptr);
}
#endif // UMPIRE_ENABLE_HIP

// Test typed reallocate with element count that's not a power of 2
TEST(Reallocate, TypedReallocateOddElements)
{
  constexpr std::size_t initial_elements = 77;  // Not a power of 2
  constexpr std::size_t final_elements = 133;   // Not a power of 2

  auto& rm = umpire::ResourceManager::getInstance();
  auto allocator = rm.getAllocator("HOST");

  // Allocate buffer for long*
  long* ptr = static_cast<long*>(allocator.allocate(initial_elements * sizeof(long)));

  // Fill with recognizable pattern
  for (std::size_t i = 0; i < initial_elements; ++i) {
    ptr[i] = static_cast<long>(i * 1000 + 7);
  }

  // Reallocate using element count
  ptr = umpire::reallocate(&ptr, final_elements);

  // Verify all original data preserved
  for (std::size_t i = 0; i < initial_elements; ++i) {
    ASSERT_EQ(ptr[i], static_cast<long>(i * 1000 + 7))
        << "Element " << i << " corrupted during reallocate";
  }

  // Verify we can write to new elements
  for (std::size_t i = initial_elements; i < final_elements; ++i) {
    ptr[i] = static_cast<long>(i * 2000);
  }

  // Verify writes succeeded
  for (std::size_t i = initial_elements; i < final_elements; ++i) {
    ASSERT_EQ(ptr[i], static_cast<long>(i * 2000)) << "New element " << i << " write failed";
  }

  // Cleanup
  allocator.deallocate(ptr);
}

TEST(Reallocate, HostReallocateAsync)
{
  constexpr std::size_t initial_size = 512;
  constexpr std::size_t final_size = 1024;
  constexpr int num_initial_elements = initial_size / sizeof(int);
  constexpr int num_final_elements = final_size / sizeof(int);

  auto& rm = umpire::ResourceManager::getInstance();
  auto allocator = rm.getAllocator("HOST");

  // Allocate initial buffer
  int* ptr = static_cast<int*>(allocator.allocate(initial_size));

  // Fill with test data
  for (int i = 0; i < num_initial_elements; ++i) {
    ptr[i] = i + 800;
  }

  // Create host resource for async operation
  camp::resources::Resource host_ctx{camp::resources::Host{}};

  // Reallocate to larger size asynchronously
  auto event = umpire::reallocate(&ptr, num_final_elements, host_ctx);

  // Wait for async operation to complete
  static_cast<camp::resources::Event>(event).wait();

  // Verify original data is preserved
  for (int i = 0; i < num_initial_elements; ++i) {
    ASSERT_EQ(ptr[i], i + 800) << "Async reallocate lost data at index " << i;
  }

  // Fill new portion
  for (int i = num_initial_elements; i < num_final_elements; ++i) {
    ptr[i] = i + 900;
  }

  // Verify new data can be written
  for (int i = num_initial_elements; i < num_final_elements; ++i) {
    ASSERT_EQ(ptr[i], i + 900) << "New data write failed at index " << i;
  }

  // Cleanup
  allocator.deallocate(ptr);
}

#if defined(UMPIRE_ENABLE_CUDA)
TEST(Reallocate, CudaReallocateAsync)
{
  constexpr std::size_t initial_size = 512;
  constexpr std::size_t final_size = 1024;
  constexpr int num_initial_elements = initial_size / sizeof(int);
  constexpr int num_final_elements = final_size / sizeof(int);

  auto& rm = umpire::ResourceManager::getInstance();
  auto cuda_allocator = rm.getAllocator("DEVICE");
  auto host_allocator = rm.getAllocator("HOST");

  // Allocate device buffer
  int* device_ptr = static_cast<int*>(cuda_allocator.allocate(initial_size));

  // Allocate host buffer for data setup
  int* host_ptr = static_cast<int*>(host_allocator.allocate(final_size));

  // Fill host buffer with test data
  for (int i = 0; i < num_initial_elements; ++i) {
    host_ptr[i] = i + 1000;
  }

  // Copy initial data to device
  umpire::copy(host_ptr, device_ptr, num_initial_elements);

  // Create CUDA resource for async operation
  camp::resources::Resource cuda_ctx{camp::resources::Cuda{}};

  // Reallocate device memory asynchronously
  auto event = umpire::reallocate(&device_ptr, num_final_elements, cuda_ctx);

  // Wait for async operation to complete
  static_cast<camp::resources::Event>(event).wait();

  // Copy back to host for verification
  umpire::copy(device_ptr, host_ptr, num_final_elements);

  // Verify original data is preserved
  for (int i = 0; i < num_initial_elements; ++i) {
    ASSERT_EQ(host_ptr[i], i + 1000) << "Async CUDA reallocate lost data at index " << i;
  }

  // Cleanup
  cuda_allocator.deallocate(device_ptr);
  host_allocator.deallocate(host_ptr);
}
#endif

//------------------------------------------------------------------------------
// Platform-by-value overload tests
//------------------------------------------------------------------------------

TEST(Reallocate, ExplicitPlatformHost)
{
  constexpr std::size_t initial_elements = 128;
  constexpr std::size_t final_elements = 256;

  auto& rm = umpire::ResourceManager::getInstance();
  auto allocator = rm.getAllocator("HOST");

  int* ptr = static_cast<int*>(allocator.allocate(initial_elements * sizeof(int)));

  // Fill with test data
  for (std::size_t i = 0; i < initial_elements; ++i) {
    ptr[i] = static_cast<int>(i + 777);
  }

  // Reallocate using explicit platform
  ptr = umpire::reallocate(camp::resources::Platform::host, &ptr, final_elements);

  // Verify original data is preserved
  for (std::size_t i = 0; i < initial_elements; ++i) {
    ASSERT_EQ(ptr[i], static_cast<int>(i + 777)) << "Data lost at index " << i;
  }

  allocator.deallocate(ptr);
}

TEST(Reallocate, ExplicitPlatformHostAsync)
{
  constexpr std::size_t initial_elements = 128;
  constexpr std::size_t final_elements = 256;

  auto& rm = umpire::ResourceManager::getInstance();
  auto allocator = rm.getAllocator("HOST");

  int* ptr = static_cast<int*>(allocator.allocate(initial_elements * sizeof(int)));

  // Fill with test data
  for (std::size_t i = 0; i < initial_elements; ++i) {
    ptr[i] = static_cast<int>(i + 888);
  }

  // Create host resource
  camp::resources::Resource host_ctx{camp::resources::Host{}};

  // Async reallocate using explicit platform
  auto event = umpire::reallocate(camp::resources::Platform::host, &ptr, final_elements, host_ctx);

  // Wait for completion
  static_cast<camp::resources::Event>(event).wait();

  // Verify original data is preserved
  for (std::size_t i = 0; i < initial_elements; ++i) {
    ASSERT_EQ(ptr[i], static_cast<int>(i + 888)) << "Data lost at index " << i;
  }

  allocator.deallocate(ptr);
}

TEST(Reallocate, ExplicitPlatformHostVoidPtr)
{
  constexpr std::size_t initial_size = 256;
  constexpr std::size_t final_size = 512;

  auto& rm = umpire::ResourceManager::getInstance();
  auto allocator = rm.getAllocator("HOST");

  void* ptr = allocator.allocate(initial_size);

  // Fill with pattern
  std::memset(ptr, 0xCD, initial_size);

  // Reallocate using explicit platform and void*
  ptr = umpire::reallocate(camp::resources::Platform::host, &ptr, final_size);

  // Verify pattern is preserved
  unsigned char* byte_ptr = static_cast<unsigned char*>(ptr);
  for (std::size_t i = 0; i < initial_size; ++i) {
    ASSERT_EQ(byte_ptr[i], 0xCD) << "Pattern lost at byte " << i;
  }

  allocator.deallocate(ptr);
}

#if defined(UMPIRE_ENABLE_CUDA)
TEST(Reallocate, ExplicitPlatformCuda)
{
  constexpr std::size_t initial_elements = 128;
  constexpr std::size_t final_elements = 256;

  auto& rm = umpire::ResourceManager::getInstance();
  auto cuda_allocator = rm.getAllocator("DEVICE");
  auto host_allocator = rm.getAllocator("HOST");

  int* device_ptr = static_cast<int*>(cuda_allocator.allocate(initial_elements * sizeof(int)));
  int* host_ptr = static_cast<int*>(host_allocator.allocate(final_elements * sizeof(int)));

  // Fill host buffer with test data
  for (std::size_t i = 0; i < initial_elements; ++i) {
    host_ptr[i] = static_cast<int>(i + 555);
  }

  // Copy to device
  umpire::copy(host_ptr, device_ptr, initial_elements);

  // Reallocate using explicit platform
  device_ptr = umpire::reallocate(camp::resources::Platform::cuda, &device_ptr, final_elements);

  // Copy back to verify
  umpire::copy(device_ptr, host_ptr, final_elements);

  // Verify original data is preserved
  for (std::size_t i = 0; i < initial_elements; ++i) {
    ASSERT_EQ(host_ptr[i], static_cast<int>(i + 555)) << "Data lost at index " << i;
  }

  cuda_allocator.deallocate(device_ptr);
  host_allocator.deallocate(host_ptr);
}

TEST(Reallocate, ExplicitPlatformCudaAsync)
{
  constexpr std::size_t initial_elements = 128;
  constexpr std::size_t final_elements = 256;

  auto& rm = umpire::ResourceManager::getInstance();
  auto cuda_allocator = rm.getAllocator("DEVICE");
  auto host_allocator = rm.getAllocator("HOST");

  int* device_ptr = static_cast<int*>(cuda_allocator.allocate(initial_elements * sizeof(int)));
  int* host_ptr = static_cast<int*>(host_allocator.allocate(final_elements * sizeof(int)));

  // Fill host buffer with test data
  for (std::size_t i = 0; i < initial_elements; ++i) {
    host_ptr[i] = static_cast<int>(i + 666);
  }

  // Copy to device
  umpire::copy(host_ptr, device_ptr, initial_elements);

  // Create CUDA resource
  camp::resources::Resource cuda_ctx{camp::resources::Cuda{}};

  // Async reallocate using explicit platform
  auto event = umpire::reallocate(camp::resources::Platform::cuda, &device_ptr, final_elements, cuda_ctx);

  // Wait for completion
  static_cast<camp::resources::Event>(event).wait();

  // Copy back to verify
  umpire::copy(device_ptr, host_ptr, final_elements);

  // Verify original data is preserved
  for (std::size_t i = 0; i < initial_elements; ++i) {
    ASSERT_EQ(host_ptr[i], static_cast<int>(i + 666)) << "Data lost at index " << i;
  }

  cuda_allocator.deallocate(device_ptr);
  host_allocator.deallocate(host_ptr);
}
#endif
