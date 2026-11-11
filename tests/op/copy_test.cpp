//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
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

  int* source_ptr = static_cast<int*>(allocator.allocate(size));
  int* dest_ptr = static_cast<int*>(allocator.allocate(size));

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<int> distribution(1, 100);

  for (int i = 0; i < num_elements; ++i) {
    source_ptr[i] = distribution(gen);
  }

  std::memset(dest_ptr, 0, size);

  umpire::copy(source_ptr, dest_ptr, num_elements);

  for (int i = 0; i < num_elements; ++i) {
    ASSERT_EQ(source_ptr[i], dest_ptr[i]) << "Data mismatch at index " << i;
  }

  allocator.deallocate(source_ptr);
  allocator.deallocate(dest_ptr);
}

TEST(Copy, TypedHostToHost)
{
  constexpr std::size_t num_elements = 1024;

  auto& rm = umpire::ResourceManager::getInstance();
  auto allocator = rm.getAllocator("HOST");

  float* source_ptr = static_cast<float*>(allocator.allocate(num_elements * sizeof(float)));
  float* dest_ptr = static_cast<float*>(allocator.allocate(num_elements * sizeof(float)));

  for (std::size_t i = 0; i < num_elements; ++i) {
    source_ptr[i] = static_cast<float>(i) + 0.5f;
  }

  std::memset(dest_ptr, 0, num_elements * sizeof(float));

  umpire::copy(source_ptr, dest_ptr, num_elements);

  for (std::size_t i = 0; i < num_elements; ++i) {
    ASSERT_FLOAT_EQ(source_ptr[i], dest_ptr[i]) << "Data mismatch at index " << i;
  }

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

  int* source_ptr = static_cast<int*>(allocator.allocate(size));
  int* dest_ptr = static_cast<int*>(allocator.allocate(size));

  for (int i = 0; i < num_elements; ++i) {
    source_ptr[i] = i + 100;
    dest_ptr[i] = i;
  }

  umpire::copy(source_ptr, dest_ptr, partial_elements);

  for (int i = 0; i < partial_elements; ++i) {
    ASSERT_EQ(source_ptr[i], dest_ptr[i]) << "Data mismatch at index " << i;
  }

  for (int i = partial_elements; i < num_elements; ++i) {
    ASSERT_EQ(dest_ptr[i], i) << "Data unexpectedly changed at index " << i;
  }

  allocator.deallocate(source_ptr);
  allocator.deallocate(dest_ptr);
}

TEST(Copy, ZeroSize)
{
  auto& rm = umpire::ResourceManager::getInstance();
  auto allocator = rm.getAllocator("HOST");

  void* source_ptr = allocator.allocate(1024);
  void* dest_ptr = allocator.allocate(1024);

  std::memset(source_ptr, 0xAA, 1024);
  std::memset(dest_ptr, 0xBB, 1024);

  umpire::copy(source_ptr, dest_ptr, 0);

  ASSERT_EQ(*static_cast<unsigned char*>(dest_ptr), 0xBB);

  allocator.deallocate(source_ptr);
  allocator.deallocate(dest_ptr);
}

// Test to validate that typed copy uses element counts, not bytes
TEST(Copy, TypedCopySemantics)
{
  constexpr std::size_t num_elements = 100;

  auto& rm = umpire::ResourceManager::getInstance();
  auto allocator = rm.getAllocator("HOST");

  int* source = static_cast<int*>(allocator.allocate(num_elements * sizeof(int)));
  int* dest = static_cast<int*>(allocator.allocate(num_elements * sizeof(int)));

  for (std::size_t i = 0; i < num_elements; ++i) {
    source[i] = static_cast<int>(i * 7 + 13);
  }

  std::memset(dest, 0, num_elements * sizeof(int));

  // Copy using element count (not bytes)
  umpire::copy(source, dest, num_elements);

  for (std::size_t i = 0; i < num_elements; ++i) {
    ASSERT_EQ(dest[i], static_cast<int>(i * 7 + 13)) << "Element " << i << " not copied correctly";
  }

  allocator.deallocate(source);
  allocator.deallocate(dest);
}

// Test to validate void* copy uses bytes, not elements
TEST(Copy, VoidVsTypedSemantics)
{
  constexpr std::size_t byte_count = 100;

  auto& rm = umpire::ResourceManager::getInstance();
  auto allocator = rm.getAllocator("HOST");

  void* void_src = allocator.allocate(byte_count);
  void* void_dst = allocator.allocate(byte_count);

  std::memset(void_src, 0xAB, byte_count);
  std::memset(void_dst, 0xCD, byte_count);

  umpire::copy(void_src, void_dst, byte_count);

  unsigned char* src_bytes = static_cast<unsigned char*>(void_src);
  unsigned char* dst_bytes = static_cast<unsigned char*>(void_dst);
  for (std::size_t i = 0; i < byte_count; ++i) {
    ASSERT_EQ(dst_bytes[i], src_bytes[i]) << "Byte " << i << " not copied correctly";
  }

  allocator.deallocate(void_src);
  allocator.deallocate(void_dst);
}

#if defined(UMPIRE_ENABLE_BOUNDS_CHECKS)
// Test that typed copy bounds checks use len * sizeof(T), not raw len
TEST(Copy, TypedOutOfBounds)
{
  constexpr std::size_t num_elements = 16;

  auto& rm = umpire::ResourceManager::getInstance();
  auto allocator = rm.getAllocator("HOST");

  double* source_ptr = static_cast<double*>(allocator.allocate(num_elements * sizeof(double)));
  double* dest_ptr = static_cast<double*>(allocator.allocate(num_elements * sizeof(double)));

  ASSERT_NO_THROW(umpire::copy(source_ptr, dest_ptr, num_elements));

  // One element past the end overruns both buffers by sizeof(double) bytes
  ASSERT_THROW(umpire::copy(source_ptr, dest_ptr, num_elements + 1), umpire::runtime_error);

  allocator.deallocate(source_ptr);
  allocator.deallocate(dest_ptr);
}

TEST(Copy, TypedOutOfBoundsSmallerDestination)
{
  constexpr std::size_t src_elements = 16;
  constexpr std::size_t dst_elements = 8;

  auto& rm = umpire::ResourceManager::getInstance();
  auto allocator = rm.getAllocator("HOST");

  // Allocate a source buffer larger than the destination buffer
  double* source_ptr = static_cast<double*>(allocator.allocate(src_elements * sizeof(double)));
  double* dest_ptr = static_cast<double*>(allocator.allocate(dst_elements * sizeof(double)));

  ASSERT_NO_THROW(umpire::copy(source_ptr, dest_ptr, dst_elements));

  // Copy that fits the source but not the destination throws
  ASSERT_THROW(umpire::copy(source_ptr, dest_ptr, dst_elements + 1), umpire::runtime_error);
  ASSERT_THROW(umpire::copy(source_ptr, dest_ptr, src_elements), umpire::runtime_error);

  allocator.deallocate(source_ptr);
  allocator.deallocate(dest_ptr);
}
#endif // UMPIRE_ENABLE_BOUNDS_CHECKS

TEST(Copy, HostToHostAsync)
{
  constexpr std::size_t size = 1024;
  constexpr int num_elements = size / sizeof(int);

  auto& rm = umpire::ResourceManager::getInstance();
  auto allocator = rm.getAllocator("HOST");

  int* source_ptr = static_cast<int*>(allocator.allocate(size));
  int* dest_ptr = static_cast<int*>(allocator.allocate(size));

  for (int i = 0; i < num_elements; ++i) {
    source_ptr[i] = i + 500;
  }

  std::memset(dest_ptr, 0, size);

  camp::resources::Resource host_ctx{camp::resources::Host{}};

  auto event = umpire::copy(source_ptr, dest_ptr, num_elements, host_ctx);

  static_cast<camp::resources::Event>(event).wait();

  for (int i = 0; i < num_elements; ++i) {
    ASSERT_EQ(source_ptr[i], dest_ptr[i]) << "Async copy failed at index " << i;
  }

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

  int* device_ptr = static_cast<int*>(cuda_allocator.allocate(size));
  int* host_ptr = static_cast<int*>(host_allocator.allocate(size));
  int* temp_host = static_cast<int*>(host_allocator.allocate(size));

  for (int i = 0; i < num_elements; ++i) {
    temp_host[i] = i + 600;
  }

  umpire::copy(temp_host, device_ptr, num_elements);

  std::memset(host_ptr, 0, size);

  camp::resources::Resource cuda_ctx{camp::resources::Cuda{}};

  auto event = umpire::copy(device_ptr, host_ptr, num_elements, cuda_ctx);

  static_cast<camp::resources::Event>(event).wait();

  for (int i = 0; i < num_elements; ++i) {
    ASSERT_EQ(host_ptr[i], i + 600) << "Async CUDA->Host copy failed at index " << i;
  }

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

  int* device_ptr = static_cast<int*>(cuda_allocator.allocate(size));
  int* host_src = static_cast<int*>(host_allocator.allocate(size));
  int* host_verify = static_cast<int*>(host_allocator.allocate(size));

  for (int i = 0; i < num_elements; ++i) {
    host_src[i] = i + 700;
  }

  camp::resources::Resource cuda_ctx{camp::resources::Cuda{}};

  auto event = umpire::copy(host_src, device_ptr, num_elements, cuda_ctx);

  static_cast<camp::resources::Event>(event).wait();

  umpire::copy(device_ptr, host_verify, num_elements);

  for (int i = 0; i < num_elements; ++i) {
    ASSERT_EQ(host_verify[i], i + 700) << "Async Host->CUDA copy failed at index " << i;
  }

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

  for (std::size_t i = 0; i < num_elements; ++i) {
    source_ptr[i] = static_cast<int>(i * 3 + 42);
  }

  std::memset(dest_ptr, 0, num_elements * sizeof(int));

  umpire::copy(camp::resources::Platform::host, camp::resources::Platform::host, source_ptr, dest_ptr, num_elements);

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

  for (std::size_t i = 0; i < num_elements; ++i) {
    source_ptr[i] = static_cast<int>(i * 5 + 17);
  }

  std::memset(dest_ptr, 0, num_elements * sizeof(int));

  camp::resources::Resource host_ctx{camp::resources::Host{}};

  auto event = umpire::copy(camp::resources::Platform::host, camp::resources::Platform::host, source_ptr, dest_ptr,
                            num_elements, host_ctx);

  static_cast<camp::resources::Event>(event).wait();

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

  for (std::size_t i = 0; i < num_elements; ++i) {
    host_src[i] = static_cast<int>(i * 7 + 99);
  }

  umpire::copy(host_src, device_ptr, num_elements);

  std::memset(host_dst, 0, num_elements * sizeof(int));

  umpire::copy(camp::resources::Platform::cuda, camp::resources::Platform::host, device_ptr, host_dst, num_elements);

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

  for (std::size_t i = 0; i < num_elements; ++i) {
    host_src[i] = static_cast<int>(i * 11 + 13);
  }

  umpire::copy(camp::resources::Platform::host, camp::resources::Platform::cuda, host_src, device_ptr, num_elements);

  umpire::copy(device_ptr, host_verify, num_elements);

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

  for (std::size_t i = 0; i < num_elements; ++i) {
    host_src[i] = static_cast<int>(i * 13 + 79);
  }
  umpire::copy(host_src, device_ptr, num_elements);

  std::memset(host_dst, 0, num_elements * sizeof(int));

  camp::resources::Resource cuda_ctx{camp::resources::Cuda{}};

  auto event = umpire::copy(camp::resources::Platform::cuda, camp::resources::Platform::host, device_ptr, host_dst,
                            num_elements, cuda_ctx);

  static_cast<camp::resources::Event>(event).wait();

  for (std::size_t i = 0; i < num_elements; ++i) {
    ASSERT_EQ(host_dst[i], static_cast<int>(i * 13 + 79)) << "Data mismatch at index " << i;
  }

  cuda_allocator.deallocate(device_ptr);
  host_allocator.deallocate(host_src);
  host_allocator.deallocate(host_dst);
}
#endif
