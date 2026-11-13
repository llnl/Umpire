//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////

#include <cstring>
#include <exception>
#include <memory>
#include <optional>
#include <vector>

#include "gtest/gtest.h"
#include "umpire/ResourceManager.hpp"
#include "umpire/op.hpp"

#if defined(UMPIRE_ENABLE_SYCL)

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

//
// Basic Copy Operations
//

TEST(SyclOps, DeviceToDeviceCopySync)
{
  auto& rm = umpire::ResourceManager::getInstance();
  auto sycl_alloc = try_get_allocator(rm, "DEVICE");

  if (!sycl_alloc) {
    GTEST_SKIP() << "No SYCL DEVICE allocator available in this build";
  }

  constexpr std::size_t num_bytes = 1024;

  void* src = sycl_alloc->allocate(num_bytes);
  void* dst = sycl_alloc->allocate(num_bytes);

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

  sycl_alloc->deallocate(src);
  sycl_alloc->deallocate(dst);
  rm.getAllocator("HOST").deallocate(host_buf);
  rm.getAllocator("HOST").deallocate(verify_buf);
}

TEST(SyclOps, DeviceToDeviceCopyAsync)
{
  auto& rm = umpire::ResourceManager::getInstance();
  auto sycl_alloc = try_get_allocator(rm, "DEVICE");

  if (!sycl_alloc) {
    GTEST_SKIP() << "No SYCL DEVICE allocator available in this build";
  }

  constexpr std::size_t num_bytes = 2048;

  void* src = sycl_alloc->allocate(num_bytes);
  void* dst = sycl_alloc->allocate(num_bytes);

  std::vector<char> host_data(num_bytes, 99);
  void* host_buf = rm.getAllocator("HOST").allocate(num_bytes);
  std::memcpy(host_buf, host_data.data(), num_bytes);
  umpire::copy(host_buf, src, num_bytes);

  camp::resources::Resource sycl_ctx{camp::resources::Sycl{}};

  auto event = umpire::copy(src, dst, num_bytes, sycl_ctx);

  static_cast<camp::resources::Event>(event).wait();

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

TEST(SyclOps, HostToDeviceCopySync)
{
  auto& rm = umpire::ResourceManager::getInstance();
  auto sycl_alloc = try_get_allocator(rm, "DEVICE");

  if (!sycl_alloc) {
    GTEST_SKIP() << "No SYCL DEVICE allocator available in this build";
  }

  constexpr std::size_t num_bytes = 512;

  void* host_src = rm.getAllocator("HOST").allocate(num_bytes);
  void* device_dst = sycl_alloc->allocate(num_bytes);

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
  sycl_alloc->deallocate(device_dst);
  rm.getAllocator("HOST").deallocate(host_verify);
}

TEST(SyclOps, HostToDeviceCopyAsync)
{
  auto& rm = umpire::ResourceManager::getInstance();
  auto sycl_alloc = try_get_allocator(rm, "DEVICE");

  if (!sycl_alloc) {
    GTEST_SKIP() << "No SYCL DEVICE allocator available in this build";
  }

  constexpr std::size_t num_bytes = 1024;

  void* host_src = rm.getAllocator("HOST").allocate(num_bytes);
  void* device_dst = sycl_alloc->allocate(num_bytes);

  std::vector<int> host_data(num_bytes / sizeof(int), 12345);
  std::memcpy(host_src, host_data.data(), num_bytes);

  camp::resources::Resource sycl_ctx{camp::resources::Sycl{}};

  auto event = umpire::copy(host_src, device_dst, num_bytes, sycl_ctx);
  static_cast<camp::resources::Event>(event).wait();

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

TEST(SyclOps, DeviceToHostCopySync)
{
  auto& rm = umpire::ResourceManager::getInstance();
  auto sycl_alloc = try_get_allocator(rm, "DEVICE");

  if (!sycl_alloc) {
    GTEST_SKIP() << "No SYCL DEVICE allocator available in this build";
  }

  constexpr std::size_t num_elements = 256;
  constexpr std::size_t num_bytes = num_elements * sizeof(double);

  void* device_src = sycl_alloc->allocate(num_bytes);
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

  sycl_alloc->deallocate(device_src);
  rm.getAllocator("HOST").deallocate(host_dst);
  rm.getAllocator("HOST").deallocate(host_temp);
}

TEST(SyclOps, DeviceToHostCopyAsync)
{
  auto& rm = umpire::ResourceManager::getInstance();
  auto sycl_alloc = try_get_allocator(rm, "DEVICE");

  if (!sycl_alloc) {
    GTEST_SKIP() << "No SYCL DEVICE allocator available in this build";
  }

  constexpr std::size_t num_elements = 128;
  constexpr std::size_t num_bytes = num_elements * sizeof(float);

  void* device_src = sycl_alloc->allocate(num_bytes);
  void* host_dst = rm.getAllocator("HOST").allocate(num_bytes);

  std::vector<float> host_data(num_elements, 2.71828f);
  void* host_temp = rm.getAllocator("HOST").allocate(num_bytes);
  std::memcpy(host_temp, host_data.data(), num_bytes);
  umpire::copy(host_temp, device_src, num_bytes);

  camp::resources::Resource sycl_ctx{camp::resources::Sycl{}};

  auto event = umpire::copy(device_src, host_dst, num_bytes, sycl_ctx);
  static_cast<camp::resources::Event>(event).wait();

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

TEST(SyclOps, MemsetSync)
{
  auto& rm = umpire::ResourceManager::getInstance();
  auto sycl_alloc = try_get_allocator(rm, "DEVICE");

  if (!sycl_alloc) {
    GTEST_SKIP() << "No SYCL DEVICE allocator available in this build";
  }

  constexpr std::size_t num_bytes = 2048;

  void* device_ptr = sycl_alloc->allocate(num_bytes);

  umpire::memset(device_ptr, 0xAB, num_bytes);

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

TEST(SyclOps, MemsetAsync)
{
  auto& rm = umpire::ResourceManager::getInstance();
  auto sycl_alloc = try_get_allocator(rm, "DEVICE");

  if (!sycl_alloc) {
    GTEST_SKIP() << "No SYCL DEVICE allocator available in this build";
  }

  constexpr std::size_t num_bytes = 4096;

  void* device_ptr = sycl_alloc->allocate(num_bytes);

  camp::resources::Resource sycl_ctx{camp::resources::Sycl{}};

  auto event = umpire::memset(device_ptr, 0x55, num_bytes, sycl_ctx);
  static_cast<camp::resources::Event>(event).wait();

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

TEST(SyclOps, DeviceMemsetTypedInt)
{
  auto& rm = umpire::ResourceManager::getInstance();
  auto sycl_alloc = try_get_allocator(rm, "DEVICE");

  if (!sycl_alloc) {
    GTEST_SKIP() << "No SYCL DEVICE allocator available in this build";
  }

  constexpr std::size_t num_elements = 512;
  constexpr std::size_t num_bytes = num_elements * sizeof(int);

  int* device_ptr = static_cast<int*>(sycl_alloc->allocate(num_bytes));

  // SYCL device_memset requires resource context
  camp::resources::Resource sycl_ctx{camp::resources::Sycl{}};

  auto event = umpire::device_memset(device_ptr, 42, num_elements, sycl_ctx);
  static_cast<camp::resources::Event>(event).wait();

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

TEST(SyclOps, DeviceMemsetTypedFloat)
{
  auto& rm = umpire::ResourceManager::getInstance();
  auto sycl_alloc = try_get_allocator(rm, "DEVICE");

  if (!sycl_alloc) {
    GTEST_SKIP() << "No SYCL DEVICE allocator available in this build";
  }

  constexpr std::size_t num_elements = 256;
  constexpr std::size_t num_bytes = num_elements * sizeof(float);

  float* device_ptr = static_cast<float*>(sycl_alloc->allocate(num_bytes));

  // SYCL device_memset requires resource context
  camp::resources::Resource sycl_ctx{camp::resources::Sycl{}};

  auto event = umpire::device_memset(device_ptr, 9.99f, num_elements, sycl_ctx);
  static_cast<camp::resources::Event>(event).wait();

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

TEST(SyclOps, PrefetchSync)
{
  auto& rm = umpire::ResourceManager::getInstance();
  // Prefetch requires USM shared (managed) memory, so use the UM allocator.
  auto sycl_alloc = try_get_allocator(rm, "UM");

  if (!sycl_alloc) {
    GTEST_SKIP() << "No SYCL UM allocator available in this build";
  }

  constexpr std::size_t num_bytes = 1024;

  void* device_ptr = sycl_alloc->allocate(num_bytes);

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

TEST(SyclOps, ZeroSizeCopy)
{
  auto& rm = umpire::ResourceManager::getInstance();
  auto sycl_alloc = try_get_allocator(rm, "DEVICE");

  if (!sycl_alloc) {
    GTEST_SKIP() << "No SYCL DEVICE allocator available in this build";
  }

  void* src = sycl_alloc->allocate(1024);
  void* dst = sycl_alloc->allocate(1024);

  EXPECT_NO_THROW(umpire::copy(src, dst, 0));

  sycl_alloc->deallocate(src);
  sycl_alloc->deallocate(dst);
}

TEST(SyclOps, ExplicitPlatformAPI)
{
  auto& rm = umpire::ResourceManager::getInstance();
  auto sycl_alloc = try_get_allocator(rm, "DEVICE");

  if (!sycl_alloc) {
    GTEST_SKIP() << "No SYCL DEVICE allocator available in this build";
  }

  constexpr std::size_t num_bytes = 512;

  void* src = sycl_alloc->allocate(num_bytes);
  void* dst = sycl_alloc->allocate(num_bytes);

  std::vector<char> host_data(num_bytes, 66);
  void* host_temp = rm.getAllocator("HOST").allocate(num_bytes);
  std::memcpy(host_temp, host_data.data(), num_bytes);
  umpire::copy(host_temp, src, num_bytes);

  camp::resources::Resource sycl_ctx{camp::resources::Sycl{}};

  auto event = umpire::copy(src, dst, num_bytes, sycl_ctx);
  static_cast<camp::resources::Event>(event).wait();

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

TEST(SyclOps, ExternalUSMCopyWithResource)
{
  sycl::queue queue{sycl::property_list{sycl::property::queue::in_order{}}};
  if (!queue.get_device().has(sycl::aspect::usm_device_allocations)) {
    GTEST_SKIP() << "Device does not support USM device allocations";
  }
  constexpr std::size_t count = 32;
  auto free_usm = [&queue](int* ptr) { sycl::free(ptr, queue); };
  std::unique_ptr<int, decltype(free_usm)> src(sycl::malloc_device<int>(count + 2, queue), free_usm);
  std::unique_ptr<int, decltype(free_usm)> dst(sycl::malloc_device<int>(count + 2, queue), free_usm);
  ASSERT_NE(src.get(), nullptr);
  ASSERT_NE(dst.get(), nullptr);
  auto& rm = umpire::ResourceManager::getInstance();
  ASSERT_FALSE(rm.hasAllocator(src.get()));
  ASSERT_FALSE(rm.hasAllocator(dst.get()));
  camp::resources::Resource ctx{camp::resources::Sycl::SyclFromQueue(queue)};
  using camp::resources::Platform;
  std::vector<int> input(count, 12345);
  std::vector<int> output(count, 0);

  auto to_device = umpire::copy(Platform::host, Platform::sycl, input.data(), src.get() + 1, count, ctx);
  static_cast<camp::resources::Event>(to_device).wait();
  auto to_device2 = umpire::copy(Platform::sycl, Platform::sycl, src.get() + 1, dst.get() + 1, count, ctx);
  static_cast<camp::resources::Event>(to_device2).wait();
  auto to_host = umpire::copy(Platform::sycl, Platform::host, dst.get() + 1, output.data(), count, ctx);
  static_cast<camp::resources::Event>(to_host).wait();
  EXPECT_EQ(output, input);
}

TEST(SyclOps, ExternalUSMMemsetWithResource)
{
  sycl::queue queue{sycl::property_list{sycl::property::queue::in_order{}}};
  if (!queue.get_device().has(sycl::aspect::usm_device_allocations)) {
    GTEST_SKIP() << "Device does not support USM device allocations";
  }
  constexpr std::size_t count = 32;
  auto free_usm = [&queue](unsigned char* ptr) { sycl::free(ptr, queue); };
  std::unique_ptr<unsigned char, decltype(free_usm)> ptr(sycl::malloc_device<unsigned char>(count, queue), free_usm);
  ASSERT_NE(ptr.get(), nullptr);
  ASSERT_FALSE(umpire::ResourceManager::getInstance().hasAllocator(ptr.get()));
  camp::resources::Resource ctx{camp::resources::Sycl::SyclFromQueue(queue)};
  auto event = umpire::memset(camp::resources::Platform::sycl, ptr.get(), 0xAB, count, ctx);
  static_cast<camp::resources::Event>(event).wait();
  std::vector<unsigned char> output(count, 0);
  queue.memcpy(output.data(), ptr.get(), count).wait_and_throw();
  EXPECT_EQ(output, std::vector<unsigned char>(count, 0xAB));
}

TEST(SyclOps, ExternalPointersRequireResourceAndMatchingContext)
{
  int external[2] = {1, 2};
  int output[2] = {};
  using camp::resources::Platform;
  EXPECT_THROW(umpire::copy(Platform::sycl, Platform::host, external, output, 2), umpire::resource_error);
  EXPECT_THROW(umpire::memset(Platform::sycl, external, 0, 2), umpire::resource_error);

  sycl::queue queue{sycl::property_list{sycl::property::queue::in_order{}}};
  camp::resources::Resource ctx{camp::resources::Sycl::SyclFromQueue(queue)};
  EXPECT_THROW(umpire::memset(Platform::sycl, external, 0, 2, ctx), umpire::resource_error);
  if (!queue.get_device().has(sycl::aspect::usm_device_allocations)) {
    return;
  }
  auto free_usm = [&queue](int* ptr) { sycl::free(ptr, queue); };
  std::unique_ptr<int, decltype(free_usm)> ptr(sycl::malloc_device<int>(2, queue), free_usm);
  ASSERT_NE(ptr.get(), nullptr);
  sycl::queue other_queue(sycl::context(queue.get_device()), queue.get_device(),
                          sycl::property_list{sycl::property::queue::in_order{}});
  ASSERT_NE(queue.get_context(), other_queue.get_context());
  camp::resources::Resource other_ctx{camp::resources::Sycl::SyclFromQueue(other_queue)};
  EXPECT_THROW(umpire::copy(Platform::sycl, Platform::host, ptr.get(), output, 2, other_ctx), umpire::resource_error);
}

TEST(SyclOps, TrackedAllocationKeepsItsContextWithDifferentResource)
{
  auto& rm = umpire::ResourceManager::getInstance();
  auto allocator = try_get_allocator(rm, "DEVICE");
  if (!allocator || allocator->getAllocationStrategy()->getPlatform() != umpire::Platform::sycl) {
    GTEST_SKIP() << "No SYCL DEVICE allocator available";
  }
  constexpr std::size_t count = 32;
  auto deallocate = [&allocator](unsigned char* ptr) { allocator->deallocate(ptr); };
  std::unique_ptr<unsigned char, decltype(deallocate)> ptr(static_cast<unsigned char*>(allocator->allocate(count)),
                                                           deallocate);
  auto& allocation_queue = umpire::op::detail::get_queue_for_ptr(ptr.get());
  sycl::queue other_queue(sycl::context(allocation_queue.get_device()), allocation_queue.get_device(),
                          sycl::property_list{sycl::property::queue::in_order{}});
  ASSERT_NE(allocation_queue.get_context(), other_queue.get_context());
  camp::resources::Resource ctx{camp::resources::Sycl::SyclFromQueue(other_queue)};
  using camp::resources::Platform;
  auto fill = umpire::memset(Platform::sycl, ptr.get(), 0x55, count, ctx);
  static_cast<camp::resources::Event>(fill).wait();
  std::vector<unsigned char> output(count, 0);
  auto copy = umpire::copy(Platform::sycl, Platform::host, ptr.get(), output.data(), count, ctx);
  static_cast<camp::resources::Event>(copy).wait();
  EXPECT_EQ(output, std::vector<unsigned char>(count, 0x55));
}

#endif // UMPIRE_ENABLE_SYCL
