//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-25, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#include <algorithm>
#include <cstring>
#include <exception>
#include <optional>
#include <random>

#include "gtest/gtest.h"
#include "umpire/Allocator.hpp"
#include "umpire/ResourceManager.hpp"
#include "umpire/op.hpp"

namespace {
std::optional<umpire::Allocator> try_get_allocator(umpire::ResourceManager& rm, const char* name)
{
  try {
    return rm.getAllocator(name);
  } catch (const std::exception&) {
    return std::nullopt;
  }
}
} // namespace

TEST(DeviceMemset, TypedDeviceMemsetInt)
{
  constexpr std::size_t num_elements = 256;
  constexpr int value = 34;

  auto& rm = umpire::ResourceManager::getInstance();
  auto device_allocator = try_get_allocator(rm, "DEVICE");
  if (!device_allocator) {
    GTEST_SKIP() << "No DEVICE allocator available in this build";
  }
  auto host_allocator = rm.getAllocator("HOST");

  // Allocate device buffer for integers
  int* device_ptr = static_cast<int*>(device_allocator->allocate(num_elements * sizeof(int)));

  // Allocate host buffer for initialization and verification
  int* host_ptr = static_cast<int*>(host_allocator.allocate(num_elements * sizeof(int)));

  // Fill host buffer with pattern first
  for (std::size_t i = 0; i < num_elements; ++i) {
    host_ptr[i] = static_cast<int>(i);
  }

  // Copy initial pattern to device
  umpire::copy(host_ptr, device_ptr, num_elements);

  // Use umpire::device_memset to set each element to value (pass element count for typed pointer)
  umpire::device_memset(device_ptr, value, num_elements);

  // Copy back to host for verification
  umpire::copy(device_ptr, host_ptr, num_elements);

  // Verify each int is set to the actual value
  for (std::size_t i = 0; i < num_elements; ++i) {
    ASSERT_EQ(host_ptr[i], 34) << "Device memset failed at element " << i;
  }

  // Cleanup
  device_allocator->deallocate(device_ptr);
  host_allocator.deallocate(host_ptr);
}

TEST(DeviceMemset, PartialDeviceMemset)
{
  constexpr std::size_t total_elements = 1024;
  constexpr std::size_t partial_elements = 512;
  constexpr int set_value = 99;
  constexpr int initial_value = -1;

  auto& rm = umpire::ResourceManager::getInstance();
  auto device_allocator = try_get_allocator(rm, "DEVICE");
  if (!device_allocator) {
    GTEST_SKIP() << "No DEVICE allocator available in this build";
  }
  auto host_allocator = rm.getAllocator("HOST");

  // Allocate device buffer for integers
  int* device_ptr = static_cast<int*>(device_allocator->allocate(total_elements * sizeof(int)));

  // Allocate host buffer for initialization and verification
  int* host_ptr = static_cast<int*>(host_allocator.allocate(total_elements * sizeof(int)));

  // Fill entire host buffer with initial value
  for (std::size_t i = 0; i < total_elements; ++i) {
    host_ptr[i] = initial_value;
  }

  // Copy initial values to device
  umpire::copy(host_ptr, device_ptr, total_elements);

  // Use umpire::device_memset on first half only
  umpire::device_memset(device_ptr, set_value, partial_elements);

  // Copy back to host for verification
  umpire::copy(device_ptr, host_ptr, total_elements);

  // Verify first half is set to new value
  for (std::size_t i = 0; i < partial_elements; ++i) {
    ASSERT_EQ(host_ptr[i], set_value) << "Device memset failed at element " << i;
  }

  // Verify second half is unchanged
  for (std::size_t i = partial_elements; i < total_elements; ++i) {
    ASSERT_EQ(host_ptr[i], initial_value) << "Unchanged region modified at element " << i;
  }

  // Cleanup
  device_allocator->deallocate(device_ptr);
  host_allocator.deallocate(host_ptr);
}

TEST(DeviceMemset, ZeroSize)
{
  constexpr std::size_t num_elements = 1024;
  constexpr int initial_value = 42;
  constexpr int memset_value = 99;

  auto& rm = umpire::ResourceManager::getInstance();
  auto device_allocator = try_get_allocator(rm, "DEVICE");
  if (!device_allocator) {
    GTEST_SKIP() << "No DEVICE allocator available in this build";
  }
  auto host_allocator = rm.getAllocator("HOST");

  // Allocate device buffer for integers
  int* device_ptr = static_cast<int*>(device_allocator->allocate(num_elements * sizeof(int)));

  // Allocate host buffer for initialization and verification
  int* host_ptr = static_cast<int*>(host_allocator.allocate(num_elements * sizeof(int)));

  // Fill host buffer with initial value
  for (std::size_t i = 0; i < num_elements; ++i) {
    host_ptr[i] = initial_value;
  }

  // Copy initial values to device
  umpire::copy(host_ptr, device_ptr, num_elements);

  // Use umpire::device_memset with zero size - should be no-op
  umpire::device_memset(device_ptr, memset_value, 0);

  // Copy back to host for verification
  umpire::copy(device_ptr, host_ptr, num_elements);

  // Verify buffer is unchanged
  for (std::size_t i = 0; i < num_elements; ++i) {
    ASSERT_EQ(host_ptr[i], initial_value) << "Zero-size device memset modified element " << i;
  }

  // Cleanup
  device_allocator->deallocate(device_ptr);
  host_allocator.deallocate(host_ptr);
}

TEST(DeviceMemset, DifferentValues)
{
  constexpr std::size_t num_elements = 256;
  constexpr int test_values[] = {0, 1, -1, 42, 100, -100, 1000, -1000};

  auto& rm = umpire::ResourceManager::getInstance();
  auto device_allocator = try_get_allocator(rm, "DEVICE");
  if (!device_allocator) {
    GTEST_SKIP() << "No DEVICE allocator available in this build";
  }
  auto host_allocator = rm.getAllocator("HOST");

  // Allocate device buffer for integers
  int* device_ptr = static_cast<int*>(device_allocator->allocate(num_elements * sizeof(int)));

  // Allocate host buffer for verification
  int* host_ptr = static_cast<int*>(host_allocator.allocate(num_elements * sizeof(int)));

  // Test each value
  for (int test_value : test_values) {
    // Use umpire::device_memset with test value on device memory
    umpire::device_memset(device_ptr, test_value, num_elements);

    // Copy back to host for verification
    umpire::copy(device_ptr, host_ptr, num_elements);

    // Verify all elements are set to test value
    for (std::size_t i = 0; i < num_elements; ++i) {
      ASSERT_EQ(host_ptr[i], test_value) << "Device memset failed for value " << test_value
                                         << " at element " << i;
    }
  }

  // Cleanup
  device_allocator->deallocate(device_ptr);
  host_allocator.deallocate(host_ptr);
}

#if defined(UMPIRE_ENABLE_CUDA)
TEST(DeviceMemset, CudaDeviceMemset)
{
  constexpr std::size_t num_elements = 1024;
  constexpr int value = 42;

  auto& rm = umpire::ResourceManager::getInstance();
  auto cuda_allocator = try_get_allocator(rm, "DEVICE");
  if (!cuda_allocator) {
    GTEST_SKIP() << "No DEVICE allocator available in this build";
  }
  auto host_allocator = rm.getAllocator("HOST");

  // Allocate device buffer for integers
  int* device_ptr = static_cast<int*>(cuda_allocator->allocate(num_elements * sizeof(int)));

  // Allocate host buffer for verification
  int* host_ptr = static_cast<int*>(host_allocator.allocate(num_elements * sizeof(int)));

  // Use umpire::device_memset on device memory
  umpire::device_memset(device_ptr, value, num_elements);

  // Copy back to host for verification
  umpire::copy(device_ptr, host_ptr, num_elements);

  // Verify the memset was successful
  for (std::size_t i = 0; i < num_elements; ++i) {
    ASSERT_EQ(host_ptr[i], value) << "CUDA device memset failed at element " << i;
  }

  // Cleanup
  cuda_allocator->deallocate(device_ptr);
  host_allocator.deallocate(host_ptr);
}

TEST(DeviceMemset, ExplicitCudaDeviceMemset)
{
  constexpr std::size_t num_elements = 1024;
  constexpr double value = 3.14;

  auto& rm = umpire::ResourceManager::getInstance();
  auto cuda_allocator = try_get_allocator(rm, "DEVICE");
  if (!cuda_allocator) {
    GTEST_SKIP() << "No DEVICE allocator available in this build";
  }
  auto host_allocator = rm.getAllocator("HOST");

  // Allocate device buffer for doubles
  double* device_ptr = static_cast<double*>(cuda_allocator->allocate(num_elements * sizeof(double)));

  // Allocate host buffer for verification
  double* host_ptr = static_cast<double*>(host_allocator.allocate(num_elements * sizeof(double)));

  // Use explicit platform device_memset
  umpire::device_memset<umpire::cuda>(device_ptr, value, num_elements);

  // Copy back to host for verification
  umpire::copy(device_ptr, host_ptr, num_elements);

  // Verify the memset was successful
  for (std::size_t i = 0; i < num_elements; ++i) {
    ASSERT_DOUBLE_EQ(host_ptr[i], value) << "Explicit CUDA device memset failed at element " << i;
  }

  // Cleanup
  cuda_allocator->deallocate(device_ptr);
  host_allocator.deallocate(host_ptr);
}
#endif // UMPIRE_ENABLE_CUDA

#if defined(UMPIRE_ENABLE_HIP)
TEST(DeviceMemset, HipDeviceMemset)
{
  constexpr std::size_t num_elements = 1024;
  constexpr int value = 77;

  auto& rm = umpire::ResourceManager::getInstance();
  auto hip_allocator = try_get_allocator(rm, "DEVICE");
  if (!hip_allocator) {
    GTEST_SKIP() << "No DEVICE allocator available in this build";
  }
  auto host_allocator = rm.getAllocator("HOST");

  // Allocate device buffer for integers
  int* device_ptr = static_cast<int*>(hip_allocator->allocate(num_elements * sizeof(int)));

  // Allocate host buffer for verification
  int* host_ptr = static_cast<int*>(host_allocator.allocate(num_elements * sizeof(int)));

  // Use umpire::device_memset on device memory
  umpire::device_memset(device_ptr, value, num_elements);

  // Copy back to host for verification
  umpire::copy(device_ptr, host_ptr, num_elements);

  // Verify the memset was successful
  for (std::size_t i = 0; i < num_elements; ++i) {
    ASSERT_EQ(host_ptr[i], value) << "HIP device memset failed at element " << i;
  }

  // Cleanup
  hip_allocator->deallocate(device_ptr);
  host_allocator.deallocate(host_ptr);
}

TEST(DeviceMemset, ExplicitHipDeviceMemset)
{
  constexpr std::size_t num_elements = 1024;
  constexpr float value = 2.718f;

  auto& rm = umpire::ResourceManager::getInstance();
  auto hip_allocator = try_get_allocator(rm, "DEVICE");
  if (!hip_allocator) {
    GTEST_SKIP() << "No DEVICE allocator available in this build";
  }
  auto host_allocator = rm.getAllocator("HOST");

  // Allocate device buffer for floats
  float* device_ptr = static_cast<float*>(hip_allocator->allocate(num_elements * sizeof(float)));

  // Allocate host buffer for verification
  float* host_ptr = static_cast<float*>(host_allocator.allocate(num_elements * sizeof(float)));

  // Use explicit platform device_memset
  umpire::device_memset<umpire::hip>(device_ptr, value, num_elements);

  // Copy back to host for verification
  umpire::copy(device_ptr, host_ptr, num_elements);

  // Verify the memset was successful
  for (std::size_t i = 0; i < num_elements; ++i) {
    ASSERT_FLOAT_EQ(host_ptr[i], value) << "Explicit HIP device memset failed at element " << i;
  }

  // Cleanup
  hip_allocator->deallocate(device_ptr);
  host_allocator.deallocate(host_ptr);
}
#endif // UMPIRE_ENABLE_HIP

//------------------------------------------------------------------------------
// Platform-by-value overload tests
//------------------------------------------------------------------------------

#if defined(UMPIRE_ENABLE_CUDA)
TEST(DeviceMemset, ExplicitPlatformCuda)
{
  constexpr std::size_t num_elements = 512;
  constexpr int value = 123;

  auto& rm = umpire::ResourceManager::getInstance();
  auto cuda_allocator = try_get_allocator(rm, "DEVICE");
  if (!cuda_allocator) {
    GTEST_SKIP() << "No DEVICE allocator available in this build";
  }
  auto host_allocator = rm.getAllocator("HOST");

  int* device_ptr = static_cast<int*>(cuda_allocator->allocate(num_elements * sizeof(int)));
  int* host_ptr = static_cast<int*>(host_allocator.allocate(num_elements * sizeof(int)));

  // Use explicit platform device_memset
  umpire::device_memset(camp::resources::Platform::cuda, device_ptr, value, num_elements);

  // Copy back to verify
  umpire::copy(device_ptr, host_ptr, num_elements);

  // Verify
  for (std::size_t i = 0; i < num_elements; ++i) {
    ASSERT_EQ(host_ptr[i], value) << "Device memset failed at element " << i;
  }

  cuda_allocator->deallocate(device_ptr);
  host_allocator.deallocate(host_ptr);
}

TEST(DeviceMemset, ExplicitPlatformCudaDouble)
{
  constexpr std::size_t num_elements = 512;
  constexpr double value = 9.876;

  auto& rm = umpire::ResourceManager::getInstance();
  auto cuda_allocator = try_get_allocator(rm, "DEVICE");
  if (!cuda_allocator) {
    GTEST_SKIP() << "No DEVICE allocator available in this build";
  }
  auto host_allocator = rm.getAllocator("HOST");

  double* device_ptr = static_cast<double*>(cuda_allocator->allocate(num_elements * sizeof(double)));
  double* host_ptr = static_cast<double*>(host_allocator.allocate(num_elements * sizeof(double)));

  // Use explicit platform device_memset with double value
  umpire::device_memset(camp::resources::Platform::cuda, device_ptr, value, num_elements);

  // Copy back to verify
  umpire::copy(device_ptr, host_ptr, num_elements);

  // Verify
  for (std::size_t i = 0; i < num_elements; ++i) {
    ASSERT_DOUBLE_EQ(host_ptr[i], value) << "Device memset failed at element " << i;
  }

  cuda_allocator->deallocate(device_ptr);
  host_allocator.deallocate(host_ptr);
}
#endif // UMPIRE_ENABLE_CUDA

#if defined(UMPIRE_ENABLE_HIP)
TEST(DeviceMemset, ExplicitPlatformHip)
{
  constexpr std::size_t num_elements = 512;
  constexpr int value = 456;

  auto& rm = umpire::ResourceManager::getInstance();
  auto hip_allocator = try_get_allocator(rm, "DEVICE");
  if (!hip_allocator) {
    GTEST_SKIP() << "No DEVICE allocator available in this build";
  }
  auto host_allocator = rm.getAllocator("HOST");

  int* device_ptr = static_cast<int*>(hip_allocator->allocate(num_elements * sizeof(int)));
  int* host_ptr = static_cast<int*>(host_allocator.allocate(num_elements * sizeof(int)));

  // Use explicit platform device_memset
  umpire::device_memset(camp::resources::Platform::hip, device_ptr, value, num_elements);

  // Copy back to verify
  umpire::copy(device_ptr, host_ptr, num_elements);

  // Verify
  for (std::size_t i = 0; i < num_elements; ++i) {
    ASSERT_EQ(host_ptr[i], value) << "Device memset failed at element " << i;
  }

  hip_allocator->deallocate(device_ptr);
  host_allocator.deallocate(host_ptr);
}
#endif // UMPIRE_ENABLE_HIP
