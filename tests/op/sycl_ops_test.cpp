//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-2024, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"
#include "umpire/ResourceManager.hpp"
#include "umpire/op.hpp"

#include <optional>
#include <vector>

#if defined(UMPIRE_ENABLE_SYCL)

namespace {

// Helper to safely get allocator without throwing if unavailable
std::optional<umpire::Allocator> try_get_allocator(umpire::ResourceManager& rm, const char* name) {
  try {
    return rm.getAllocator(name);
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

} // namespace

//
// Basic Copy Operations
//

TEST(SyclOps, DeviceToDeviceCopySync) {
  auto& rm = umpire::ResourceManager::getInstance();
  auto sycl_alloc = try_get_allocator(rm, "SYCL");

  if (!sycl_alloc) {
    GTEST_SKIP() << "No SYCL allocator available in this build";
  }

  constexpr std::size_t num_bytes = 1024;

  void* src = sycl_alloc->allocate(num_bytes);
  void* dst = sycl_alloc->allocate(num_bytes);

  // Initialize source data on host
  std::vector<char> host_data(num_bytes, 42);
  void* host_buf = rm.getAllocator("HOST").allocate(num_bytes);
  std::memcpy(host_buf, host_data.data(), num_bytes);

  // Copy to device source
  umpire::copy(host_buf, src, num_bytes);

  // Device-to-device copy (synchronous)
  umpire::copy(src, dst, num_bytes);

  // Verify by copying back to host
  std::vector<char> result(num_bytes, 0);
  void* verify_buf = rm.getAllocator("HOST").allocate(num_bytes);
  umpire::copy(dst, verify_buf, num_bytes);
  std::memcpy(result.data(), verify_buf, num_bytes);

  for (std::size_t i = 0; i < num_bytes; ++i) {
    EXPECT_EQ(result[i], 42) << "Mismatch at byte " << i;
  }

  sycl_alloc->deallocate(src);
  sycl_alloc->deallocate(dst);
  rm.getAllocator("HOST").deallocate(host_buf);
  rm.getAllocator("HOST").deallocate(verify_buf);
}

TEST(SyclOps, DeviceToDeviceCopyAsync) {
  auto& rm = umpire::ResourceManager::getInstance();
  auto sycl_alloc = try_get_allocator(rm, "SYCL");

  if (!sycl_alloc) {
    GTEST_SKIP() << "No SYCL allocator available in this build";
  }

  constexpr std::size_t num_bytes = 2048;

  void* src = sycl_alloc->allocate(num_bytes);
  void* dst = sycl_alloc->allocate(num_bytes);

  // Initialize source
  std::vector<char> host_data(num_bytes, 99);
  void* host_buf = rm.getAllocator("HOST").allocate(num_bytes);
  std::memcpy(host_buf, host_data.data(), num_bytes);
  umpire::copy(host_buf, src, num_bytes);

  // Create SYCL resource for async operation
  camp::resources::Resource sycl_ctx{camp::resources::Sycl{}};

  // Async device-to-device copy
  auto event = umpire::copy(src, dst, num_bytes, sycl_ctx);

  // SYCL has true async support - must explicitly wait
  static_cast<camp::resources::Event>(event).wait();

  // Verify
  void* verify_buf = rm.getAllocator("HOST").allocate(num_bytes);
  umpire::copy(dst, verify_buf, num_bytes);

  std::vector<char> result(num_bytes, 0);
  std::memcpy(result.data(), verify_buf, num_bytes);

  for (std::size_t i = 0; i < num_bytes; ++i) {
    EXPECT_EQ(result[i], 99) << "Mismatch at byte " << i;
  }

  sycl_alloc->deallocate(src);
  sycl_alloc->deallocate(dst);
  rm.getAllocator("HOST").deallocate(host_buf);
  rm.getAllocator("HOST").deallocate(verify_buf);
}

TEST(SyclOps, HostToDeviceCopySync) {
  auto& rm = umpire::ResourceManager::getInstance();
  auto sycl_alloc = try_get_allocator(rm, "SYCL");

  if (!sycl_alloc) {
    GTEST_SKIP() << "No SYCL allocator available in this build";
  }

  constexpr std::size_t num_bytes = 512;

  void* host_src = rm.getAllocator("HOST").allocate(num_bytes);
  void* device_dst = sycl_alloc->allocate(num_bytes);

  // Initialize host data
  std::vector<char> host_data(num_bytes, 77);
  std::memcpy(host_src, host_data.data(), num_bytes);

  // Host-to-device copy (synchronous)
  umpire::copy(host_src, device_dst, num_bytes);

  // Verify by copying back
  void* host_verify = rm.getAllocator("HOST").allocate(num_bytes);
  umpire::copy(device_dst, host_verify, num_bytes);

  std::vector<char> result(num_bytes, 0);
  std::memcpy(result.data(), host_verify, num_bytes);

  for (std::size_t i = 0; i < num_bytes; ++i) {
    EXPECT_EQ(result[i], 77) << "Mismatch at byte " << i;
  }

  rm.getAllocator("HOST").deallocate(host_src);
  sycl_alloc->deallocate(device_dst);
  rm.getAllocator("HOST").deallocate(host_verify);
}

TEST(SyclOps, HostToDeviceCopyAsync) {
  auto& rm = umpire::ResourceManager::getInstance();
  auto sycl_alloc = try_get_allocator(rm, "SYCL");

  if (!sycl_alloc) {
    GTEST_SKIP() << "No SYCL allocator available in this build";
  }

  constexpr std::size_t num_bytes = 1024;

  void* host_src = rm.getAllocator("HOST").allocate(num_bytes);
  void* device_dst = sycl_alloc->allocate(num_bytes);

  // Initialize host data
  std::vector<int> host_data(num_bytes / sizeof(int), 12345);
  std::memcpy(host_src, host_data.data(), num_bytes);

  // Create SYCL resource for async operation
  camp::resources::Resource sycl_ctx{camp::resources::Sycl{}};

  // Async host-to-device copy
  auto event = umpire::copy(host_src, device_dst, num_bytes, sycl_ctx);
  static_cast<camp::resources::Event>(event).wait();

  // Verify
  void* host_verify = rm.getAllocator("HOST").allocate(num_bytes);
  umpire::copy(device_dst, host_verify, num_bytes);

  std::vector<int> result(num_bytes / sizeof(int), 0);
  std::memcpy(result.data(), host_verify, num_bytes);

  for (std::size_t i = 0; i < result.size(); ++i) {
    EXPECT_EQ(result[i], 12345) << "Mismatch at element " << i;
  }

  rm.getAllocator("HOST").deallocate(host_src);
  sycl_alloc->deallocate(device_dst);
  rm.getAllocator("HOST").deallocate(host_verify);
}

TEST(SyclOps, DeviceToHostCopySync) {
  auto& rm = umpire::ResourceManager::getInstance();
  auto sycl_alloc = try_get_allocator(rm, "SYCL");

  if (!sycl_alloc) {
    GTEST_SKIP() << "No SYCL allocator available in this build";
  }

  constexpr std::size_t num_elements = 256;
  constexpr std::size_t num_bytes = num_elements * sizeof(double);

  void* device_src = sycl_alloc->allocate(num_bytes);
  void* host_dst = rm.getAllocator("HOST").allocate(num_bytes);

  // Initialize device data via host
  std::vector<double> host_data(num_elements, 3.14159);
  void* host_temp = rm.getAllocator("HOST").allocate(num_bytes);
  std::memcpy(host_temp, host_data.data(), num_bytes);
  umpire::copy(host_temp, device_src, num_bytes);

  // Device-to-host copy (synchronous)
  umpire::copy(device_src, host_dst, num_bytes);

  // Verify
  std::vector<double> result(num_elements, 0.0);
  std::memcpy(result.data(), host_dst, num_bytes);

  for (std::size_t i = 0; i < num_elements; ++i) {
    EXPECT_DOUBLE_EQ(result[i], 3.14159) << "Mismatch at element " << i;
  }

  sycl_alloc->deallocate(device_src);
  rm.getAllocator("HOST").deallocate(host_dst);
  rm.getAllocator("HOST").deallocate(host_temp);
}

TEST(SyclOps, DeviceToHostCopyAsync) {
  auto& rm = umpire::ResourceManager::getInstance();
  auto sycl_alloc = try_get_allocator(rm, "SYCL");

  if (!sycl_alloc) {
    GTEST_SKIP() << "No SYCL allocator available in this build";
  }

  constexpr std::size_t num_elements = 128;
  constexpr std::size_t num_bytes = num_elements * sizeof(float);

  void* device_src = sycl_alloc->allocate(num_bytes);
  void* host_dst = rm.getAllocator("HOST").allocate(num_bytes);

  // Initialize device data
  std::vector<float> host_data(num_elements, 2.71828f);
  void* host_temp = rm.getAllocator("HOST").allocate(num_bytes);
  std::memcpy(host_temp, host_data.data(), num_bytes);
  umpire::copy(host_temp, device_src, num_bytes);

  // Create SYCL resource for async operation
  camp::resources::Resource sycl_ctx{camp::resources::Sycl{}};

  // Async device-to-host copy
  auto event = umpire::copy(device_src, host_dst, num_bytes, sycl_ctx);
  static_cast<camp::resources::Event>(event).wait();

  // Verify
  std::vector<float> result(num_elements, 0.0f);
  std::memcpy(result.data(), host_dst, num_bytes);

  for (std::size_t i = 0; i < num_elements; ++i) {
    EXPECT_FLOAT_EQ(result[i], 2.71828f) << "Mismatch at element " << i;
  }

  sycl_alloc->deallocate(device_src);
  rm.getAllocator("HOST").deallocate(host_dst);
  rm.getAllocator("HOST").deallocate(host_temp);
}

//
// Memset Operations
//

TEST(SyclOps, MemsetSync) {
  auto& rm = umpire::ResourceManager::getInstance();
  auto sycl_alloc = try_get_allocator(rm, "SYCL");

  if (!sycl_alloc) {
    GTEST_SKIP() << "No SYCL allocator available in this build";
  }

  constexpr std::size_t num_bytes = 2048;

  void* device_ptr = sycl_alloc->allocate(num_bytes);

  // Memset (synchronous)
  umpire::memset(device_ptr, 0xAB, num_bytes);

  // Verify by copying to host
  void* host_verify = rm.getAllocator("HOST").allocate(num_bytes);
  umpire::copy(device_ptr, host_verify, num_bytes);

  std::vector<unsigned char> result(num_bytes, 0);
  std::memcpy(result.data(), host_verify, num_bytes);

  for (std::size_t i = 0; i < num_bytes; ++i) {
    EXPECT_EQ(result[i], 0xAB) << "Mismatch at byte " << i;
  }

  sycl_alloc->deallocate(device_ptr);
  rm.getAllocator("HOST").deallocate(host_verify);
}

TEST(SyclOps, MemsetAsync) {
  auto& rm = umpire::ResourceManager::getInstance();
  auto sycl_alloc = try_get_allocator(rm, "SYCL");

  if (!sycl_alloc) {
    GTEST_SKIP() << "No SYCL allocator available in this build";
  }

  constexpr std::size_t num_bytes = 4096;

  void* device_ptr = sycl_alloc->allocate(num_bytes);

  // Create SYCL resource for async operation
  camp::resources::Resource sycl_ctx{camp::resources::Sycl{}};

  // Async memset
  auto event = umpire::memset(device_ptr, 0x55, num_bytes, sycl_ctx);
  static_cast<camp::resources::Event>(event).wait();

  // Verify
  void* host_verify = rm.getAllocator("HOST").allocate(num_bytes);
  umpire::copy(device_ptr, host_verify, num_bytes);

  std::vector<unsigned char> result(num_bytes, 0);
  std::memcpy(result.data(), host_verify, num_bytes);

  for (std::size_t i = 0; i < num_bytes; ++i) {
    EXPECT_EQ(result[i], 0x55) << "Mismatch at byte " << i;
  }

  sycl_alloc->deallocate(device_ptr);
  rm.getAllocator("HOST").deallocate(host_verify);
}

//
// Device Memset Operations (typed values)
//

TEST(SyclOps, DeviceMemsetTypedInt) {
  auto& rm = umpire::ResourceManager::getInstance();
  auto sycl_alloc = try_get_allocator(rm, "SYCL");

  if (!sycl_alloc) {
    GTEST_SKIP() << "No SYCL allocator available in this build";
  }

  constexpr std::size_t num_elements = 512;
  constexpr std::size_t num_bytes = num_elements * sizeof(int);

  int* device_ptr = static_cast<int*>(sycl_alloc->allocate(num_bytes));

  // SYCL device_memset requires resource context
  camp::resources::Resource sycl_ctx{camp::resources::Sycl{}};

  // Set all elements to value 42
  auto event = umpire::device_memset(device_ptr, 42, num_elements, sycl_ctx);
  static_cast<camp::resources::Event>(event).wait();

  // Verify by copying to host
  void* host_verify = rm.getAllocator("HOST").allocate(num_bytes);
  umpire::copy(static_cast<void*>(device_ptr), host_verify, num_bytes);

  std::vector<int> result(num_elements, 0);
  std::memcpy(result.data(), host_verify, num_bytes);

  for (std::size_t i = 0; i < num_elements; ++i) {
    EXPECT_EQ(result[i], 42) << "Mismatch at element " << i;
  }

  sycl_alloc->deallocate(device_ptr);
  rm.getAllocator("HOST").deallocate(host_verify);
}

TEST(SyclOps, DeviceMemsetTypedFloat) {
  auto& rm = umpire::ResourceManager::getInstance();
  auto sycl_alloc = try_get_allocator(rm, "SYCL");

  if (!sycl_alloc) {
    GTEST_SKIP() << "No SYCL allocator available in this build";
  }

  constexpr std::size_t num_elements = 256;
  constexpr std::size_t num_bytes = num_elements * sizeof(float);

  float* device_ptr = static_cast<float*>(sycl_alloc->allocate(num_bytes));

  // SYCL device_memset requires resource context
  camp::resources::Resource sycl_ctx{camp::resources::Sycl{}};

  // Set all elements to 9.99f
  auto event = umpire::device_memset(device_ptr, 9.99f, num_elements, sycl_ctx);
  static_cast<camp::resources::Event>(event).wait();

  // Verify
  void* host_verify = rm.getAllocator("HOST").allocate(num_bytes);
  umpire::copy(static_cast<void*>(device_ptr), host_verify, num_bytes);

  std::vector<float> result(num_elements, 0.0f);
  std::memcpy(result.data(), host_verify, num_bytes);

  for (std::size_t i = 0; i < num_elements; ++i) {
    EXPECT_FLOAT_EQ(result[i], 9.99f) << "Mismatch at element " << i;
  }

  sycl_alloc->deallocate(device_ptr);
  rm.getAllocator("HOST").deallocate(host_verify);
}

//
// Prefetch Operations
//

TEST(SyclOps, PrefetchSync) {
  auto& rm = umpire::ResourceManager::getInstance();
  auto sycl_alloc = try_get_allocator(rm, "SYCL");

  if (!sycl_alloc) {
    GTEST_SKIP() << "No SYCL allocator available in this build";
  }

  constexpr std::size_t num_bytes = 1024;

  void* device_ptr = sycl_alloc->allocate(num_bytes);

  // Initialize data
  std::vector<char> host_data(num_bytes, 88);
  void* host_temp = rm.getAllocator("HOST").allocate(num_bytes);
  std::memcpy(host_temp, host_data.data(), num_bytes);
  umpire::copy(host_temp, device_ptr, num_bytes);

  // The synchronous SYCL prefetch intentionally throws: it has no queue
  // context, so callers are redirected to the resource-taking async overload
  // (see prefetch<sycl_platform>::exec in op/sycl.hpp).
  EXPECT_THROW(umpire::prefetch(device_ptr, 0, num_bytes), umpire::runtime_error);

  // Async prefetch with a resource context is the supported path. It is a
  // performance hint and doesn't affect correctness.
  camp::resources::Resource sycl_ctx{camp::resources::Sycl{}};
  auto event = umpire::prefetch(device_ptr, 0, num_bytes, sycl_ctx);
  static_cast<camp::resources::Event>(event).wait();

  // Verify data is still intact
  void* host_verify = rm.getAllocator("HOST").allocate(num_bytes);
  umpire::copy(device_ptr, host_verify, num_bytes);

  std::vector<char> result(num_bytes, 0);
  std::memcpy(result.data(), host_verify, num_bytes);

  for (std::size_t i = 0; i < num_bytes; ++i) {
    EXPECT_EQ(result[i], 88) << "Mismatch at byte " << i;
  }

  sycl_alloc->deallocate(device_ptr);
  rm.getAllocator("HOST").deallocate(host_temp);
  rm.getAllocator("HOST").deallocate(host_verify);
}

//
// Edge Cases
//

TEST(SyclOps, ZeroSizeCopy) {
  auto& rm = umpire::ResourceManager::getInstance();
  auto sycl_alloc = try_get_allocator(rm, "SYCL");

  if (!sycl_alloc) {
    GTEST_SKIP() << "No SYCL allocator available in this build";
  }

  void* src = sycl_alloc->allocate(1024);
  void* dst = sycl_alloc->allocate(1024);

  // Zero-size copy should be a no-op and not crash
  EXPECT_NO_THROW(umpire::copy(src, dst, 0));

  sycl_alloc->deallocate(src);
  sycl_alloc->deallocate(dst);
}

TEST(SyclOps, ExplicitPlatformAPI) {
  auto& rm = umpire::ResourceManager::getInstance();
  auto sycl_alloc = try_get_allocator(rm, "SYCL");

  if (!sycl_alloc) {
    GTEST_SKIP() << "No SYCL allocator available in this build";
  }

  constexpr std::size_t num_bytes = 512;

  void* src = sycl_alloc->allocate(num_bytes);
  void* dst = sycl_alloc->allocate(num_bytes);

  // Initialize source
  std::vector<char> host_data(num_bytes, 66);
  void* host_temp = rm.getAllocator("HOST").allocate(num_bytes);
  std::memcpy(host_temp, host_data.data(), num_bytes);
  umpire::copy(host_temp, src, num_bytes);

  // Use explicit platform API through camp::resources::Platform
  camp::resources::Resource sycl_ctx{camp::resources::Sycl{}};

  // Copy using explicit resource
  auto event = umpire::copy(src, dst, num_bytes, sycl_ctx);
  static_cast<camp::resources::Event>(event).wait();

  // Verify
  void* host_verify = rm.getAllocator("HOST").allocate(num_bytes);
  umpire::copy(dst, host_verify, num_bytes);

  std::vector<char> result(num_bytes, 0);
  std::memcpy(result.data(), host_verify, num_bytes);

  for (std::size_t i = 0; i < num_bytes; ++i) {
    EXPECT_EQ(result[i], 66) << "Mismatch at byte " << i;
  }

  sycl_alloc->deallocate(src);
  sycl_alloc->deallocate(dst);
  rm.getAllocator("HOST").deallocate(host_temp);
  rm.getAllocator("HOST").deallocate(host_verify);
}

#endif // UMPIRE_ENABLE_SYCL
