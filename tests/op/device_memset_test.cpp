//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#include <exception>
#include <optional>

#include "gtest/gtest.h"
#include "umpire/Allocator.hpp"
#include "umpire/ResourceManager.hpp"
#include "umpire/op.hpp"

namespace {
// [[maybe_unused]]: on SYCL-only builds every caller below is compiled out.
[[maybe_unused]] std::optional<umpire::Allocator> try_get_allocator(umpire::ResourceManager& rm, const char* name)
{
  try {
    return rm.getAllocator(name);
  } catch (const std::exception&) {
    return std::nullopt;
  }
}
} // namespace

// The generic tests below use the synchronous umpire::device_memset, which
// throws by design on SYCL (it requires a resource context; see op/sycl.hpp).
// SYCL coverage of device_memset lives in sycl_ops_test.cpp via the async
// resource-taking overload.
#if !defined(UMPIRE_ENABLE_SYCL)
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

  int* device_ptr = static_cast<int*>(device_allocator->allocate(num_elements * sizeof(int)));

  int* host_ptr = static_cast<int*>(host_allocator.allocate(num_elements * sizeof(int)));

  for (std::size_t i = 0; i < num_elements; ++i) {
    host_ptr[i] = static_cast<int>(i);
  }

  umpire::copy(host_ptr, device_ptr, num_elements);

  // Use umpire::device_memset to set each element to value (pass element count for typed pointer)
  umpire::device_memset(device_ptr, value, num_elements);

  umpire::copy(device_ptr, host_ptr, num_elements);

  for (std::size_t i = 0; i < num_elements; ++i) {
    ASSERT_EQ(host_ptr[i], 34) << "Device memset failed at element " << i;
  }

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

  int* device_ptr = static_cast<int*>(device_allocator->allocate(total_elements * sizeof(int)));

  int* host_ptr = static_cast<int*>(host_allocator.allocate(total_elements * sizeof(int)));

  for (std::size_t i = 0; i < total_elements; ++i) {
    host_ptr[i] = initial_value;
  }

  umpire::copy(host_ptr, device_ptr, total_elements);

  umpire::device_memset(device_ptr, set_value, partial_elements);

  umpire::copy(device_ptr, host_ptr, total_elements);

  for (std::size_t i = 0; i < partial_elements; ++i) {
    ASSERT_EQ(host_ptr[i], set_value) << "Device memset failed at element " << i;
  }

  for (std::size_t i = partial_elements; i < total_elements; ++i) {
    ASSERT_EQ(host_ptr[i], initial_value) << "Unchanged region modified at element " << i;
  }

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

  int* device_ptr = static_cast<int*>(device_allocator->allocate(num_elements * sizeof(int)));

  int* host_ptr = static_cast<int*>(host_allocator.allocate(num_elements * sizeof(int)));

  for (std::size_t i = 0; i < num_elements; ++i) {
    host_ptr[i] = initial_value;
  }

  umpire::copy(host_ptr, device_ptr, num_elements);

  umpire::device_memset(device_ptr, memset_value, 0);

  umpire::copy(device_ptr, host_ptr, num_elements);

  for (std::size_t i = 0; i < num_elements; ++i) {
    ASSERT_EQ(host_ptr[i], initial_value) << "Zero-size device memset modified element " << i;
  }

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

  int* device_ptr = static_cast<int*>(device_allocator->allocate(num_elements * sizeof(int)));

  int* host_ptr = static_cast<int*>(host_allocator.allocate(num_elements * sizeof(int)));

  for (int test_value : test_values) {
    umpire::device_memset(device_ptr, test_value, num_elements);

    umpire::copy(device_ptr, host_ptr, num_elements);

    for (std::size_t i = 0; i < num_elements; ++i) {
      ASSERT_EQ(host_ptr[i], test_value) << "Device memset failed for value " << test_value << " at element " << i;
    }
  }

  device_allocator->deallocate(device_ptr);
  host_allocator.deallocate(host_ptr);
}
#endif // !UMPIRE_ENABLE_SYCL

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

  int* device_ptr = static_cast<int*>(cuda_allocator->allocate(num_elements * sizeof(int)));

  int* host_ptr = static_cast<int*>(host_allocator.allocate(num_elements * sizeof(int)));

  umpire::device_memset(device_ptr, value, num_elements);

  umpire::copy(device_ptr, host_ptr, num_elements);

  for (std::size_t i = 0; i < num_elements; ++i) {
    ASSERT_EQ(host_ptr[i], value) << "CUDA device memset failed at element " << i;
  }

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

  double* device_ptr = static_cast<double*>(cuda_allocator->allocate(num_elements * sizeof(double)));

  double* host_ptr = static_cast<double*>(host_allocator.allocate(num_elements * sizeof(double)));

  umpire::device_memset<umpire::cuda>(device_ptr, value, num_elements);

  umpire::copy(device_ptr, host_ptr, num_elements);

  for (std::size_t i = 0; i < num_elements; ++i) {
    ASSERT_DOUBLE_EQ(host_ptr[i], value) << "Explicit CUDA device memset failed at element " << i;
  }

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

  int* device_ptr = static_cast<int*>(hip_allocator->allocate(num_elements * sizeof(int)));

  int* host_ptr = static_cast<int*>(host_allocator.allocate(num_elements * sizeof(int)));

  umpire::device_memset(device_ptr, value, num_elements);

  umpire::copy(device_ptr, host_ptr, num_elements);

  for (std::size_t i = 0; i < num_elements; ++i) {
    ASSERT_EQ(host_ptr[i], value) << "HIP device memset failed at element " << i;
  }

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

  float* device_ptr = static_cast<float*>(hip_allocator->allocate(num_elements * sizeof(float)));

  float* host_ptr = static_cast<float*>(host_allocator.allocate(num_elements * sizeof(float)));

  umpire::device_memset<umpire::hip>(device_ptr, value, num_elements);

  umpire::copy(device_ptr, host_ptr, num_elements);

  for (std::size_t i = 0; i < num_elements; ++i) {
    ASSERT_FLOAT_EQ(host_ptr[i], value) << "Explicit HIP device memset failed at element " << i;
  }

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

  umpire::device_memset(camp::resources::Platform::cuda, device_ptr, value, num_elements);

  umpire::copy(device_ptr, host_ptr, num_elements);

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

  umpire::device_memset(camp::resources::Platform::cuda, device_ptr, value, num_elements);

  umpire::copy(device_ptr, host_ptr, num_elements);

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

  umpire::device_memset(camp::resources::Platform::hip, device_ptr, value, num_elements);

  umpire::copy(device_ptr, host_ptr, num_elements);

  for (std::size_t i = 0; i < num_elements; ++i) {
    ASSERT_EQ(host_ptr[i], value) << "Device memset failed at element " << i;
  }

  hip_allocator->deallocate(device_ptr);
  host_allocator.deallocate(host_ptr);
}
#endif // UMPIRE_ENABLE_HIP
