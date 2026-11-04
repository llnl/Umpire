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

  int* ptr = static_cast<int*>(allocator.allocate(initial_size));

  for (int i = 0; i < num_initial_elements; ++i) {
    ptr[i] = i + 100;
  }

  ptr = umpire::reallocate(&ptr, num_final_elements);

  for (int i = 0; i < num_initial_elements; ++i) {
    ASSERT_EQ(ptr[i], i + 100) << "Original data lost at index " << i;
  }

  for (int i = num_initial_elements; i < num_final_elements; ++i) {
    ptr[i] = i + 200;
  }

  for (int i = num_initial_elements; i < num_final_elements; ++i) {
    ASSERT_EQ(ptr[i], i + 200) << "New data write failed at index " << i;
  }

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

  int* ptr = static_cast<int*>(allocator.allocate(initial_size));

  for (int i = 0; i < num_initial_elements; ++i) {
    ptr[i] = i * 2;
  }

  ptr = umpire::reallocate(&ptr, num_final_elements);

  for (int i = 0; i < num_final_elements; ++i) {
    ASSERT_EQ(ptr[i], i * 2) << "Data corruption at index " << i << " after shrinking";
  }

  allocator.deallocate(ptr);
}

TEST(Reallocate, HostReallocateSameSize)
{
  constexpr std::size_t size = 1024;
  constexpr int num_elements = size / sizeof(int);

  auto& rm = umpire::ResourceManager::getInstance();
  auto allocator = rm.getAllocator("HOST");

  int* ptr = static_cast<int*>(allocator.allocate(size));

  for (int i = 0; i < num_elements; ++i) {
    ptr[i] = i * 3 + 42;
  }

  ptr = umpire::reallocate(&ptr, num_elements);

  for (int i = 0; i < num_elements; ++i) {
    ASSERT_EQ(ptr[i], i * 3 + 42) << "Data corruption at index " << i << " for same-size realloc";
  }

  allocator.deallocate(ptr);
}

TEST(Reallocate, HostReallocateToZero)
{
  constexpr std::size_t initial_size = 1024;

  auto& rm = umpire::ResourceManager::getInstance();
  auto allocator = rm.getAllocator("HOST");

  int* ptr = static_cast<int*>(allocator.allocate(initial_size));

  *ptr = 12345;

  ptr = static_cast<int*>(umpire::reallocate(&ptr, 0));

  ASSERT_NE(ptr, nullptr);

  allocator.deallocate(ptr);
}

TEST(Reallocate, HostReallocateFromNull)
{
  constexpr std::size_t size = 1024;
  constexpr int num_elements = size / sizeof(int);

  int* ptr = nullptr;

  // Reallocate from null - should work like malloc
  ptr = umpire::reallocate(&ptr, num_elements);
  ASSERT_NE(ptr, nullptr);

  for (int i = 0; i < static_cast<int>(size / sizeof(int)); ++i) {
    ptr[i] = i + 500;
  }

  for (int i = 0; i < static_cast<int>(size / sizeof(int)); ++i) {
    ASSERT_EQ(ptr[i], i + 500) << "Data write failed at index " << i;
  }

  auto& rm = umpire::ResourceManager::getInstance();
  rm.deallocate(ptr);
}

TEST(Reallocate, TypedReallocate)
{
  constexpr std::size_t initial_elements = 128;
  constexpr std::size_t final_elements = 256;

  auto& rm = umpire::ResourceManager::getInstance();
  auto allocator = rm.getAllocator("HOST");

  double* ptr = static_cast<double*>(allocator.allocate(initial_elements * sizeof(double)));

  for (std::size_t i = 0; i < initial_elements; ++i) {
    ptr[i] = static_cast<double>(i) + 0.5;
  }

  ptr = umpire::reallocate(&ptr, final_elements);

  for (std::size_t i = 0; i < initial_elements; ++i) {
    ASSERT_DOUBLE_EQ(ptr[i], static_cast<double>(i) + 0.5) << "Double data lost at index " << i;
  }

  allocator.deallocate(ptr);
}

TEST(Reallocate, VoidPointerReallocate)
{
  constexpr std::size_t initial_size = 512;
  constexpr std::size_t final_size = 1024;

  auto& rm = umpire::ResourceManager::getInstance();
  auto allocator = rm.getAllocator("HOST");

  void* ptr = allocator.allocate(initial_size);

  std::memset(ptr, 0xAB, initial_size);

  ptr = umpire::reallocate(&ptr, final_size);

  unsigned char* byte_ptr = static_cast<unsigned char*>(ptr);
  for (std::size_t i = 0; i < initial_size; ++i) {
    ASSERT_EQ(byte_ptr[i], 0xAB) << "Pattern lost at byte " << i;
  }

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

  int* device_ptr = static_cast<int*>(cuda_allocator.allocate(initial_size));

  int* host_ptr = static_cast<int*>(host_allocator.allocate(final_size));

  for (int i = 0; i < num_initial_elements; ++i) {
    host_ptr[i] = i + 300;
  }

  umpire::copy(host_ptr, device_ptr, num_initial_elements);

  device_ptr = umpire::reallocate(&device_ptr, num_final_elements);

  umpire::copy(device_ptr, host_ptr, num_final_elements);

  for (int i = 0; i < num_initial_elements; ++i) {
    ASSERT_EQ(host_ptr[i], i + 300) << "CUDA reallocate lost data at index " << i;
  }

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

  int* device_ptr = static_cast<int*>(hip_allocator.allocate(initial_size));

  int* host_ptr = static_cast<int*>(host_allocator.allocate(final_size));

  for (int i = 0; i < num_initial_elements; ++i) {
    host_ptr[i] = i + 400;
  }

  umpire::copy(host_ptr, device_ptr, num_initial_elements);

  device_ptr = umpire::reallocate(&device_ptr, num_final_elements);

  umpire::copy(device_ptr, host_ptr, num_final_elements);

  for (int i = 0; i < num_initial_elements; ++i) {
    ASSERT_EQ(host_ptr[i], i + 400) << "HIP reallocate lost data at index " << i;
  }

  hip_allocator.deallocate(device_ptr);
  host_allocator.deallocate(host_ptr);
}
#endif // UMPIRE_ENABLE_HIP

// Test typed reallocate with element count that's not a power of 2
TEST(Reallocate, TypedReallocateOddElements)
{
  constexpr std::size_t initial_elements = 77;
  constexpr std::size_t final_elements = 133;

  auto& rm = umpire::ResourceManager::getInstance();
  auto allocator = rm.getAllocator("HOST");

  long* ptr = static_cast<long*>(allocator.allocate(initial_elements * sizeof(long)));

  for (std::size_t i = 0; i < initial_elements; ++i) {
    ptr[i] = static_cast<long>(i * 1000 + 7);
  }

  ptr = umpire::reallocate(&ptr, final_elements);

  for (std::size_t i = 0; i < initial_elements; ++i) {
    ASSERT_EQ(ptr[i], static_cast<long>(i * 1000 + 7))
        << "Element " << i << " corrupted during reallocate";
  }

  for (std::size_t i = initial_elements; i < final_elements; ++i) {
    ptr[i] = static_cast<long>(i * 2000);
  }

  for (std::size_t i = initial_elements; i < final_elements; ++i) {
    ASSERT_EQ(ptr[i], static_cast<long>(i * 2000)) << "New element " << i << " write failed";
  }

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

  int* ptr = static_cast<int*>(allocator.allocate(initial_size));

  for (int i = 0; i < num_initial_elements; ++i) {
    ptr[i] = i + 800;
  }

  camp::resources::Resource host_ctx{camp::resources::Host{}};

  auto event = umpire::reallocate(&ptr, num_final_elements, host_ctx);

  static_cast<camp::resources::Event>(event).wait();

  for (int i = 0; i < num_initial_elements; ++i) {
    ASSERT_EQ(ptr[i], i + 800) << "Async reallocate lost data at index " << i;
  }

  for (int i = num_initial_elements; i < num_final_elements; ++i) {
    ptr[i] = i + 900;
  }

  for (int i = num_initial_elements; i < num_final_elements; ++i) {
    ASSERT_EQ(ptr[i], i + 900) << "New data write failed at index " << i;
  }

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

  int* device_ptr = static_cast<int*>(cuda_allocator.allocate(initial_size));

  int* host_ptr = static_cast<int*>(host_allocator.allocate(final_size));

  for (int i = 0; i < num_initial_elements; ++i) {
    host_ptr[i] = i + 1000;
  }

  umpire::copy(host_ptr, device_ptr, num_initial_elements);

  camp::resources::Resource cuda_ctx{camp::resources::Cuda{}};

  auto event = umpire::reallocate(&device_ptr, num_final_elements, cuda_ctx);

  static_cast<camp::resources::Event>(event).wait();

  umpire::copy(device_ptr, host_ptr, num_final_elements);

  for (int i = 0; i < num_initial_elements; ++i) {
    ASSERT_EQ(host_ptr[i], i + 1000) << "Async CUDA reallocate lost data at index " << i;
  }

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

  for (std::size_t i = 0; i < initial_elements; ++i) {
    ptr[i] = static_cast<int>(i + 777);
  }

  ptr = umpire::reallocate(camp::resources::Platform::host, &ptr, final_elements);

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

  for (std::size_t i = 0; i < initial_elements; ++i) {
    ptr[i] = static_cast<int>(i + 888);
  }

  camp::resources::Resource host_ctx{camp::resources::Host{}};

  auto event = umpire::reallocate(camp::resources::Platform::host, &ptr, final_elements, host_ctx);

  static_cast<camp::resources::Event>(event).wait();

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

  std::memset(ptr, 0xCD, initial_size);

  ptr = umpire::reallocate(camp::resources::Platform::host, &ptr, final_size);

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

  for (std::size_t i = 0; i < initial_elements; ++i) {
    host_ptr[i] = static_cast<int>(i + 555);
  }

  umpire::copy(host_ptr, device_ptr, initial_elements);

  device_ptr = umpire::reallocate(camp::resources::Platform::cuda, &device_ptr, final_elements);

  umpire::copy(device_ptr, host_ptr, final_elements);

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

  for (std::size_t i = 0; i < initial_elements; ++i) {
    host_ptr[i] = static_cast<int>(i + 666);
  }

  umpire::copy(host_ptr, device_ptr, initial_elements);

  camp::resources::Resource cuda_ctx{camp::resources::Cuda{}};

  auto event = umpire::reallocate(camp::resources::Platform::cuda, &device_ptr, final_elements, cuda_ctx);

  static_cast<camp::resources::Event>(event).wait();

  umpire::copy(device_ptr, host_ptr, final_elements);

  for (std::size_t i = 0; i < initial_elements; ++i) {
    ASSERT_EQ(host_ptr[i], static_cast<int>(i + 666)) << "Data lost at index " << i;
  }

  cuda_allocator.deallocate(device_ptr);
  host_allocator.deallocate(host_ptr);
}
#endif
