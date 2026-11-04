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
#include <memory>
#include <vector>

#if defined(UMPIRE_ENABLE_OPENMP_TARGET)

namespace {

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
// NOTE: OpenMP Target async operations are currently synchronous
// See include/umpire/op/openmp_target.hpp lines 64-80 for details
//

TEST(OpenMPTargetOps, DeviceToDeviceCopySync) {
  auto& rm = umpire::ResourceManager::getInstance();
  auto omp_alloc = try_get_allocator(rm, "DEVICE");

  if (!omp_alloc) {
    GTEST_SKIP() << "No OpenMP target DEVICE allocator available in this build";
  }

  constexpr std::size_t num_bytes = 1024;

  void* src = omp_alloc->allocate(num_bytes);
  void* dst = omp_alloc->allocate(num_bytes);

  std::vector<char> host_data(num_bytes, 42);
  void* host_buf = rm.getAllocator("HOST").allocate(num_bytes);
  std::memcpy(host_buf, host_data.data(), num_bytes);

  umpire::copy(host_buf, src, num_bytes);

  umpire::copy(src, dst, num_bytes);

  std::vector<char> result(num_bytes, 0);
  void* verify_buf = rm.getAllocator("HOST").allocate(num_bytes);
  umpire::copy(dst, verify_buf, num_bytes);
  std::memcpy(result.data(), verify_buf, num_bytes);

  for (std::size_t i = 0; i < num_bytes; ++i) {
    EXPECT_EQ(result[i], 42) << "Mismatch at byte " << i;
  }

  omp_alloc->deallocate(src);
  omp_alloc->deallocate(dst);
  rm.getAllocator("HOST").deallocate(host_buf);
  rm.getAllocator("HOST").deallocate(verify_buf);
}

TEST(OpenMPTargetOps, DeviceToDeviceCopyAsync) {
  // NOTE: OpenMP Target async operations are currently synchronous
  // See include/umpire/op/openmp_target.hpp lines 64-80 for details
  // This test verifies API compatibility, not true async behavior

  auto& rm = umpire::ResourceManager::getInstance();
  auto omp_alloc = try_get_allocator(rm, "DEVICE");

  if (!omp_alloc) {
    GTEST_SKIP() << "No OpenMP target DEVICE allocator available in this build";
  }

  constexpr std::size_t num_bytes = 2048;

  void* src = omp_alloc->allocate(num_bytes);
  void* dst = omp_alloc->allocate(num_bytes);

  std::vector<char> host_data(num_bytes, 99);
  void* host_buf = rm.getAllocator("HOST").allocate(num_bytes);
  std::memcpy(host_buf, host_data.data(), num_bytes);
  umpire::copy(host_buf, src, num_bytes);

  camp::resources::Resource omp_ctx{camp::resources::Omp{}};

  auto event = umpire::copy(src, dst, num_bytes, omp_ctx);

  static_cast<camp::resources::Event>(event).wait();

  void* verify_buf = rm.getAllocator("HOST").allocate(num_bytes);
  umpire::copy(dst, verify_buf, num_bytes);

  std::vector<char> result(num_bytes, 0);
  std::memcpy(result.data(), verify_buf, num_bytes);

  for (std::size_t i = 0; i < num_bytes; ++i) {
    EXPECT_EQ(result[i], 99) << "Mismatch at byte " << i;
  }

  omp_alloc->deallocate(src);
  omp_alloc->deallocate(dst);
  rm.getAllocator("HOST").deallocate(host_buf);
  rm.getAllocator("HOST").deallocate(verify_buf);
}

TEST(OpenMPTargetOps, HostToDeviceCopySync) {
  auto& rm = umpire::ResourceManager::getInstance();
  auto omp_alloc = try_get_allocator(rm, "DEVICE");

  if (!omp_alloc) {
    GTEST_SKIP() << "No OpenMP target DEVICE allocator available in this build";
  }

  constexpr std::size_t num_bytes = 512;

  void* host_src = rm.getAllocator("HOST").allocate(num_bytes);
  void* device_dst = omp_alloc->allocate(num_bytes);

  std::vector<char> host_data(num_bytes, 77);
  std::memcpy(host_src, host_data.data(), num_bytes);

  umpire::copy(host_src, device_dst, num_bytes);

  void* host_verify = rm.getAllocator("HOST").allocate(num_bytes);
  umpire::copy(device_dst, host_verify, num_bytes);

  std::vector<char> result(num_bytes, 0);
  std::memcpy(result.data(), host_verify, num_bytes);

  for (std::size_t i = 0; i < num_bytes; ++i) {
    EXPECT_EQ(result[i], 77) << "Mismatch at byte " << i;
  }

  rm.getAllocator("HOST").deallocate(host_src);
  omp_alloc->deallocate(device_dst);
  rm.getAllocator("HOST").deallocate(host_verify);
}

TEST(OpenMPTargetOps, HostToDeviceCopyAsync) {
  // NOTE: OpenMP Target async operations are currently synchronous
  // See include/umpire/op/openmp_target.hpp lines 64-80

  auto& rm = umpire::ResourceManager::getInstance();
  auto omp_alloc = try_get_allocator(rm, "DEVICE");

  if (!omp_alloc) {
    GTEST_SKIP() << "No OpenMP target DEVICE allocator available in this build";
  }

  constexpr std::size_t num_bytes = 1024;

  void* host_src = rm.getAllocator("HOST").allocate(num_bytes);
  void* device_dst = omp_alloc->allocate(num_bytes);

  std::vector<int> host_data(num_bytes / sizeof(int), 12345);
  std::memcpy(host_src, host_data.data(), num_bytes);

  camp::resources::Resource omp_ctx{camp::resources::Omp{}};

  auto event = umpire::copy(host_src, device_dst, num_bytes, omp_ctx);
  static_cast<camp::resources::Event>(event).wait();

  void* host_verify = rm.getAllocator("HOST").allocate(num_bytes);
  umpire::copy(device_dst, host_verify, num_bytes);

  std::vector<int> result(num_bytes / sizeof(int), 0);
  std::memcpy(result.data(), host_verify, num_bytes);

  for (std::size_t i = 0; i < result.size(); ++i) {
    EXPECT_EQ(result[i], 12345) << "Mismatch at element " << i;
  }

  rm.getAllocator("HOST").deallocate(host_src);
  omp_alloc->deallocate(device_dst);
  rm.getAllocator("HOST").deallocate(host_verify);
}

TEST(OpenMPTargetOps, DeviceToHostCopySync) {
  auto& rm = umpire::ResourceManager::getInstance();
  auto omp_alloc = try_get_allocator(rm, "DEVICE");

  if (!omp_alloc) {
    GTEST_SKIP() << "No OpenMP target DEVICE allocator available in this build";
  }

  constexpr std::size_t num_elements = 256;
  constexpr std::size_t num_bytes = num_elements * sizeof(double);

  void* device_src = omp_alloc->allocate(num_bytes);
  void* host_dst = rm.getAllocator("HOST").allocate(num_bytes);

  std::vector<double> host_data(num_elements, 3.14159);
  void* host_temp = rm.getAllocator("HOST").allocate(num_bytes);
  std::memcpy(host_temp, host_data.data(), num_bytes);
  umpire::copy(host_temp, device_src, num_bytes);

  umpire::copy(device_src, host_dst, num_bytes);

  std::vector<double> result(num_elements, 0.0);
  std::memcpy(result.data(), host_dst, num_bytes);

  for (std::size_t i = 0; i < num_elements; ++i) {
    EXPECT_DOUBLE_EQ(result[i], 3.14159) << "Mismatch at element " << i;
  }

  omp_alloc->deallocate(device_src);
  rm.getAllocator("HOST").deallocate(host_dst);
  rm.getAllocator("HOST").deallocate(host_temp);
}

TEST(OpenMPTargetOps, DeviceToHostCopyAsync) {
  // NOTE: OpenMP Target async operations are currently synchronous
  // See include/umpire/op/openmp_target.hpp lines 64-80

  auto& rm = umpire::ResourceManager::getInstance();
  auto omp_alloc = try_get_allocator(rm, "DEVICE");

  if (!omp_alloc) {
    GTEST_SKIP() << "No OpenMP target DEVICE allocator available in this build";
  }

  constexpr std::size_t num_elements = 128;
  constexpr std::size_t num_bytes = num_elements * sizeof(float);

  void* device_src = omp_alloc->allocate(num_bytes);
  void* host_dst = rm.getAllocator("HOST").allocate(num_bytes);

  std::vector<float> host_data(num_elements, 2.71828f);
  void* host_temp = rm.getAllocator("HOST").allocate(num_bytes);
  std::memcpy(host_temp, host_data.data(), num_bytes);
  umpire::copy(host_temp, device_src, num_bytes);

  camp::resources::Resource omp_ctx{camp::resources::Omp{}};

  auto event = umpire::copy(device_src, host_dst, num_bytes, omp_ctx);
  static_cast<camp::resources::Event>(event).wait();

  std::vector<float> result(num_elements, 0.0f);
  std::memcpy(result.data(), host_dst, num_bytes);

  for (std::size_t i = 0; i < num_elements; ++i) {
    EXPECT_FLOAT_EQ(result[i], 2.71828f) << "Mismatch at element " << i;
  }

  omp_alloc->deallocate(device_src);
  rm.getAllocator("HOST").deallocate(host_dst);
  rm.getAllocator("HOST").deallocate(host_temp);
}

//
// Memset Operations
//

TEST(OpenMPTargetOps, MemsetSync) {
  auto& rm = umpire::ResourceManager::getInstance();
  auto omp_alloc = try_get_allocator(rm, "DEVICE");

  if (!omp_alloc) {
    GTEST_SKIP() << "No OpenMP target DEVICE allocator available in this build";
  }

  constexpr std::size_t num_bytes = 2048;

  void* device_ptr = omp_alloc->allocate(num_bytes);

  umpire::memset(device_ptr, 0xAB, num_bytes);

  void* host_verify = rm.getAllocator("HOST").allocate(num_bytes);
  umpire::copy(device_ptr, host_verify, num_bytes);

  std::vector<unsigned char> result(num_bytes, 0);
  std::memcpy(result.data(), host_verify, num_bytes);

  for (std::size_t i = 0; i < num_bytes; ++i) {
    EXPECT_EQ(result[i], 0xAB) << "Mismatch at byte " << i;
  }

  omp_alloc->deallocate(device_ptr);
  rm.getAllocator("HOST").deallocate(host_verify);
}

TEST(OpenMPTargetOps, MemsetAsync) {
  // NOTE: OpenMP Target async operations are currently synchronous
  // See include/umpire/op/openmp_target.hpp lines 64-80

  auto& rm = umpire::ResourceManager::getInstance();
  auto omp_alloc = try_get_allocator(rm, "DEVICE");

  if (!omp_alloc) {
    GTEST_SKIP() << "No OpenMP target DEVICE allocator available in this build";
  }

  constexpr std::size_t num_bytes = 4096;

  void* device_ptr = omp_alloc->allocate(num_bytes);

  camp::resources::Resource omp_ctx{camp::resources::Omp{}};

  auto event = umpire::memset(device_ptr, 0x55, num_bytes, omp_ctx);
  static_cast<camp::resources::Event>(event).wait();

  void* host_verify = rm.getAllocator("HOST").allocate(num_bytes);
  umpire::copy(device_ptr, host_verify, num_bytes);

  std::vector<unsigned char> result(num_bytes, 0);
  std::memcpy(result.data(), host_verify, num_bytes);

  for (std::size_t i = 0; i < num_bytes; ++i) {
    EXPECT_EQ(result[i], 0x55) << "Mismatch at byte " << i;
  }

  omp_alloc->deallocate(device_ptr);
  rm.getAllocator("HOST").deallocate(host_verify);
}

//
// Device Memset Operations (typed values)
//

TEST(OpenMPTargetOps, DeviceMemsetTypedInt) {
  auto& rm = umpire::ResourceManager::getInstance();
  auto omp_alloc = try_get_allocator(rm, "DEVICE");

  if (!omp_alloc) {
    GTEST_SKIP() << "No OpenMP target DEVICE allocator available in this build";
  }

  constexpr std::size_t num_elements = 512;
  constexpr std::size_t num_bytes = num_elements * sizeof(int);

  int* device_ptr = static_cast<int*>(omp_alloc->allocate(num_bytes));

  // OpenMP Target device_memset works without resource context
  // (unlike SYCL which requires it)
  umpire::device_memset(device_ptr, 42, num_elements);

  void* host_verify = rm.getAllocator("HOST").allocate(num_bytes);
  umpire::copy(static_cast<void*>(device_ptr), host_verify, num_bytes);

  std::vector<int> result(num_elements, 0);
  std::memcpy(result.data(), host_verify, num_bytes);

  for (std::size_t i = 0; i < num_elements; ++i) {
    EXPECT_EQ(result[i], 42) << "Mismatch at element " << i;
  }

  omp_alloc->deallocate(device_ptr);
  rm.getAllocator("HOST").deallocate(host_verify);
}

//
// Edge Cases
//

TEST(OpenMPTargetOps, ZeroSizeCopy) {
  auto& rm = umpire::ResourceManager::getInstance();
  auto omp_alloc = try_get_allocator(rm, "DEVICE");

  if (!omp_alloc) {
    GTEST_SKIP() << "No OpenMP target DEVICE allocator available in this build";
  }

  void* src = omp_alloc->allocate(1024);
  void* dst = omp_alloc->allocate(1024);

  EXPECT_NO_THROW(umpire::copy(src, dst, 0));

  omp_alloc->deallocate(src);
  omp_alloc->deallocate(dst);
}

TEST(OpenMPTargetOps, AsyncIsSynchronousDocumented) {
  // This test explicitly documents the known limitation that
  // OpenMP Target async operations are currently synchronous
  // See include/umpire/op/openmp_target.hpp lines 64-80:
  //
  // "Note: CAMP's OpenMP resource event implementation currently
  //  only supports synchronous events that complete immediately."
  //
  // This means:
  // 1. Async API exists for compatibility
  // 2. Operations execute synchronously
  // 3. No performance benefit over sync operations
  // 4. Event.wait() returns immediately

  auto& rm = umpire::ResourceManager::getInstance();
  auto omp_alloc = try_get_allocator(rm, "DEVICE");

  if (!omp_alloc) {
    GTEST_SKIP() << "No OpenMP target DEVICE allocator available in this build";
  }

  constexpr std::size_t num_bytes = 256;

  void* src = omp_alloc->allocate(num_bytes);
  void* dst = omp_alloc->allocate(num_bytes);

  std::vector<char> host_data(num_bytes, 123);
  void* host_buf = rm.getAllocator("HOST").allocate(num_bytes);
  std::memcpy(host_buf, host_data.data(), num_bytes);
  umpire::copy(host_buf, src, num_bytes);

  camp::resources::Resource omp_ctx{camp::resources::Omp{}};

  auto event = umpire::copy(src, dst, num_bytes, omp_ctx);

  // At this point, the copy is already complete (synchronous execution)
  // The event.wait() call below returns immediately

  static_cast<camp::resources::Event>(event).wait();

  void* host_verify = rm.getAllocator("HOST").allocate(num_bytes);
  umpire::copy(dst, host_verify, num_bytes);

  std::vector<char> result(num_bytes, 0);
  std::memcpy(result.data(), host_verify, num_bytes);

  for (std::size_t i = 0; i < num_bytes; ++i) {
    EXPECT_EQ(result[i], 123) << "Mismatch at byte " << i;
  }

  omp_alloc->deallocate(src);
  omp_alloc->deallocate(dst);
  rm.getAllocator("HOST").deallocate(host_buf);
  rm.getAllocator("HOST").deallocate(host_verify);
}

TEST(OpenMPTargetOps, ExternalTargetCopyWithResource) {
  const int device = omp_get_num_devices() > 0 ? 0 : omp_get_initial_device();
  constexpr std::size_t count = 32;
  auto free_target = [device](int* ptr) { omp_target_free(ptr, device); };
  std::unique_ptr<int, decltype(free_target)> src(
      static_cast<int*>(omp_target_alloc((count + 2) * sizeof(int), device)), free_target);
  std::unique_ptr<int, decltype(free_target)> dst(
      static_cast<int*>(omp_target_alloc((count + 2) * sizeof(int), device)), free_target);
  ASSERT_NE(src.get(), nullptr);
  ASSERT_NE(dst.get(), nullptr);
  auto& rm = umpire::ResourceManager::getInstance();
  ASSERT_FALSE(rm.hasAllocator(src.get()));
  ASSERT_FALSE(rm.hasAllocator(dst.get()));
  camp::resources::Resource ctx{camp::resources::Omp(-1, device)};
  using camp::resources::Platform;
  std::vector<int> input(count, 12345);
  std::vector<int> output(count, 0);

  auto to_device = umpire::copy(Platform::host, Platform::omp_target, input.data(), src.get() + 1, count, ctx);
  static_cast<camp::resources::Event>(to_device).wait();
  auto to_device2 = umpire::copy(Platform::omp_target, Platform::omp_target,
                                src.get() + 1, dst.get() + 1, count, ctx);
  static_cast<camp::resources::Event>(to_device2).wait();
  auto to_host = umpire::copy(Platform::omp_target, Platform::host, dst.get() + 1, output.data(), count, ctx);
  static_cast<camp::resources::Event>(to_host).wait();
  EXPECT_EQ(output, input);
}

TEST(OpenMPTargetOps, ExternalTargetMemsetWithResource) {
  const int device = omp_get_num_devices() > 0 ? 0 : omp_get_initial_device();
  constexpr std::size_t count = 32;
  auto free_target = [device](unsigned char* ptr) { omp_target_free(ptr, device); };
  std::unique_ptr<unsigned char, decltype(free_target)> ptr(
      static_cast<unsigned char*>(omp_target_alloc(count, device)), free_target);
  ASSERT_NE(ptr.get(), nullptr);
  ASSERT_FALSE(umpire::ResourceManager::getInstance().hasAllocator(ptr.get()));
  camp::resources::Resource ctx{camp::resources::Omp(-1, device)};
  auto event = umpire::memset(camp::resources::Platform::omp_target, ptr.get(), 0xAB, count, ctx);
  static_cast<camp::resources::Event>(event).wait();
  std::vector<unsigned char> output(count, 0);
  ASSERT_EQ(omp_target_memcpy(output.data(), ptr.get(), count, 0, 0, omp_get_initial_device(), device), 0);
  EXPECT_EQ(output, std::vector<unsigned char>(count, 0xAB));
}

TEST(OpenMPTargetOps, ExternalPointersRequireResource) {
  int external[2] = {1, 2};
  int output[2] = {};
  using camp::resources::Platform;
  EXPECT_THROW(umpire::copy(Platform::omp_target, Platform::host, external, output, 2),
               umpire::resource_error);
  EXPECT_THROW(umpire::memset(Platform::omp_target, external, 0, 2), umpire::resource_error);
  EXPECT_THROW(umpire::device_memset(Platform::omp_target, external, 0, 2), umpire::resource_error);
}

TEST(OpenMPTargetOps, TrackedDeviceOverridesResource) {
  auto& rm = umpire::ResourceManager::getInstance();
  auto allocator = try_get_allocator(rm, "DEVICE");
  if (!allocator || allocator->getAllocationStrategy()->getPlatform() != umpire::Platform::omp_target) {
    GTEST_SKIP() << "No OpenMP target DEVICE allocator available";
  }
  constexpr std::size_t count = 32;
  auto deallocate = [&allocator](unsigned char* ptr) { allocator->deallocate(ptr); };
  std::unique_ptr<unsigned char, decltype(deallocate)> ptr(
      static_cast<unsigned char*>(allocator->allocate(count)), deallocate);
  camp::resources::Resource ctx{camp::resources::Omp(-1, omp_get_initial_device())};
  EXPECT_EQ(umpire::op::detail::get_device_id(ptr.get(), &ctx),
            allocator->getAllocationStrategy()->getTraits().id);
  using camp::resources::Platform;
  auto fill = umpire::memset(Platform::omp_target, ptr.get(), 0x55, count, ctx);
  static_cast<camp::resources::Event>(fill).wait();
  std::vector<unsigned char> output(count, 0);
  auto copy = umpire::copy(Platform::omp_target, Platform::host, ptr.get(), output.data(), count, ctx);
  static_cast<camp::resources::Event>(copy).wait();
  EXPECT_EQ(output, std::vector<unsigned char>(count, 0x55));
}

#endif // UMPIRE_ENABLE_OPENMP_TARGET
