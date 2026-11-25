//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#include <sstream>
#include <vector>

#include "gtest/gtest.h"
#include "umpire/event/event.hpp"
#include "umpire/event/event_store.hpp"
#include "umpire/op.hpp"
#include "umpire/strategy/NamedAllocationStrategy.hpp"

namespace {

std::string pointer_string(void* ptr)
{
  std::ostringstream stream;
  stream << ptr;
  return stream.str();
}

class CapturingStore : public umpire::event::event_store {
 public:
  void insert(const umpire::event::event& event) override
  {
    events.push_back(event);
  }
  void insert(const umpire::event::allocate&) override
  {
  }
  void insert(const umpire::event::named_allocate&) override
  {
  }
  void insert(const umpire::event::allocate_resource&) override
  {
  }
  void insert(const umpire::event::deallocate&) override
  {
  }
  void insert(const umpire::event::deallocate_resource&) override
  {
  }
  std::vector<umpire::event::event> get_events() override
  {
    return events;
  }

  std::vector<umpire::event::event> events;
};

// Restore the singleton recorder even when a fatal assertion exits a test.
class RecorderCapture {
 public:
  explicit RecorderCapture(CapturingStore& store)
      : recorder(umpire::event::recorder_factory::get_recorder()), original(recorder)
  {
    recorder = umpire::event::event_store_recorder(&store);
  }
  ~RecorderCapture()
  {
    recorder = original;
  }

 private:
  umpire::event::event_store_recorder& recorder;
  umpire::event::event_store_recorder original;
};

class OperationRecordingTest : public ::testing::Test {
 protected:
  void SetUp() override
  {
    ASSERT_TRUE(umpire::event::event_build_enabled)
        << "Run this target with UMPIRE_EVENTS=1 or UMPIRE_REPLAY=1 set before process startup";
  }

  std::vector<umpire::event::event> operations(const CapturingStore& store)
  {
    std::vector<umpire::event::event> result;
    for (const auto& event : store.events) {
      if (event.cat == umpire::event::category::operation &&
          (event.name == "copy" || event.name == "memset" || event.name == "reallocate" || event.name == "move")) {
        result.push_back(event);
      }
    }
    return result;
  }

  void check_reallocate(const std::vector<umpire::event::event>& events, void* old_ptr, void* new_ptr,
                        std::size_t bytes, umpire::Allocator allocator, bool async)
  {
    ASSERT_EQ(2, events.size());
    const auto& begin = events[0];
    const auto& end = events[1];
    EXPECT_EQ("reallocate", begin.name);
    EXPECT_EQ("reallocate", end.name);
    EXPECT_EQ(pointer_string(old_ptr), begin.string_args.at("current_ptr"));
    EXPECT_EQ(bytes, begin.numeric_args.at("size"));
    EXPECT_EQ("true", begin.tags.at("replay"));
    EXPECT_EQ(async ? 1u : 0u, begin.tags.count("async"));
    EXPECT_EQ(0, begin.string_args.count("new_ptr"));
    EXPECT_EQ(pointer_string(new_ptr), end.string_args.at("new_ptr"));
    EXPECT_EQ(0, end.string_args.count("current_ptr"));
    EXPECT_EQ(0, end.tags.count("replay"));
    EXPECT_EQ(0, end.tags.count("async"));
    for (const auto& event : events) {
      EXPECT_EQ(pointer_string(allocator.getAllocationStrategy()), event.string_args.at("allocator_ref"));
      EXPECT_EQ(allocator.getName(), event.tags.at("allocator_name"));
    }
  }
};

TEST_F(OperationRecordingTest, TrackedCopyRecordsBytesOffsetsAndAllocators)
{
  auto allocator = umpire::ResourceManager::getInstance().getAllocator("HOST");
  auto src = static_cast<int*>(allocator.allocate(8 * sizeof(int)));
  auto dst = static_cast<int*>(allocator.allocate(8 * sizeof(int)));
  CapturingStore store;
  RecorderCapture capture(store);
  umpire::copy(src + 1, dst + 2, 3);
  auto events = operations(store);
  ASSERT_EQ(1, events.size());
  const auto& event = events[0];
  EXPECT_EQ("copy", event.name);
  EXPECT_EQ(pointer_string(src + 1), event.string_args.at("src"));
  EXPECT_EQ(pointer_string(dst + 2), event.string_args.at("dst"));
  EXPECT_EQ(sizeof(int), event.numeric_args.at("src_offset"));
  EXPECT_EQ(2 * sizeof(int), event.numeric_args.at("dst_offset"));
  EXPECT_EQ(3 * sizeof(int), event.numeric_args.at("size"));
  EXPECT_EQ(pointer_string(allocator.getAllocationStrategy()), event.string_args.at("src_allocator_ref"));
  EXPECT_EQ(pointer_string(allocator.getAllocationStrategy()), event.string_args.at("dst_allocator_ref"));
  EXPECT_EQ(allocator.getName(), event.tags.at("src_allocator_name"));
  EXPECT_EQ(allocator.getName(), event.tags.at("dst_allocator_name"));
  EXPECT_EQ("true", event.tags.at("replay"));
  EXPECT_EQ(0, event.tags.count("async"));
  allocator.deallocate(src);
  allocator.deallocate(dst);
}

TEST_F(OperationRecordingTest, TrackedMemsetRecordsRequestedBytes)
{
  auto allocator = umpire::ResourceManager::getInstance().getAllocator("HOST");
  auto ptr = static_cast<int*>(allocator.allocate(8 * sizeof(int)));
  CapturingStore store;
  RecorderCapture capture(store);
  umpire::memset(ptr + 1, 7, 2);
  auto events = operations(store);
  ASSERT_EQ(1, events.size());
  const auto& event = events[0];
  EXPECT_EQ("memset", event.name);
  EXPECT_EQ(pointer_string(ptr + 1), event.string_args.at("ptr"));
  EXPECT_EQ(7, event.numeric_args.at("value"));
  EXPECT_EQ(2 * sizeof(int), event.numeric_args.at("size"));
  EXPECT_EQ(pointer_string(allocator.getAllocationStrategy()), event.string_args.at("allocator_ref"));
  EXPECT_EQ(allocator.getName(), event.tags.at("allocator_name"));
  EXPECT_EQ("true", event.tags.at("replay"));
  EXPECT_EQ(0, event.tags.count("async"));
  allocator.deallocate(ptr);
}

TEST_F(OperationRecordingTest, TrackedAsyncOperationsCarryAsyncTag)
{
  auto allocator = umpire::ResourceManager::getInstance().getAllocator("HOST");
  void* src = allocator.allocate(32);
  void* dst = allocator.allocate(32);
  camp::resources::Resource resource{camp::resources::Host{}};
  CapturingStore store;
  RecorderCapture capture(store);
  umpire::memset(src, 1, 7, resource);
  umpire::copy(src, dst, 7, resource);
  auto events = operations(store);
  ASSERT_EQ(2, events.size());
  EXPECT_EQ("memset", events[0].name);
  EXPECT_EQ("copy", events[1].name);
  for (const auto& event : events) {
    EXPECT_EQ(7, event.numeric_args.at("size"));
    EXPECT_EQ("true", event.tags.at("async"));
    EXPECT_EQ("true", event.tags.at("replay"));
  }
  allocator.deallocate(src);
  allocator.deallocate(dst);
}

TEST_F(OperationRecordingTest, TrackedReallocateRecordsPairWithoutInternalCopy)
{
  auto allocator = umpire::ResourceManager::getInstance().getAllocator("HOST");
  auto ptr = static_cast<int*>(allocator.allocate(2 * sizeof(int)));
  void* old_ptr = ptr;
  CapturingStore store;
  RecorderCapture capture(store);
  umpire::reallocate(&ptr, 4);
  check_reallocate(operations(store), old_ptr, ptr, 4 * sizeof(int), allocator, false);
  allocator.deallocate(ptr);
}

TEST_F(OperationRecordingTest, TrackedAsyncReallocateRecordsPairWithoutInternalCopy)
{
  auto allocator = umpire::ResourceManager::getInstance().getAllocator("HOST");
  void* ptr = allocator.allocate(16);
  void* old_ptr = ptr;
  camp::resources::Resource resource{camp::resources::Host{}};
  CapturingStore store;
  RecorderCapture capture(store);
  umpire::reallocate(&ptr, 32, resource);
  check_reallocate(operations(store), old_ptr, ptr, 32, allocator, true);
  allocator.deallocate(ptr);
}

TEST_F(OperationRecordingTest, TrackedNullAndZeroReallocateRecordPairs)
{
  auto allocator = umpire::ResourceManager::getInstance().getDefaultAllocator();
  void* ptr = nullptr;
  CapturingStore store;
  RecorderCapture capture(store);
  umpire::reallocate(&ptr, 16);
  check_reallocate(operations(store), nullptr, ptr, 16, allocator, false);
  store.events.clear();
  void* old_ptr = ptr;
  umpire::reallocate(&ptr, 0);
  check_reallocate(operations(store), old_ptr, ptr, 0, allocator, false);
  allocator.deallocate(ptr);
}

TEST_F(OperationRecordingTest, ResourceManagerWrappersRecordOnlyTheirBoundary)
{
  auto& rm = umpire::ResourceManager::getInstance();
  auto allocator = rm.getAllocator("HOST");
  void* src = allocator.allocate(16);
  void* dst = allocator.allocate(16);
  CapturingStore store;
  RecorderCapture capture(store);
  rm.memset(src, 1, 16);
  rm.copy(dst, src, 16);
  auto events = operations(store);
  ASSERT_EQ(2, events.size());
  EXPECT_EQ("memset", events[0].name);
  EXPECT_EQ("copy", events[1].name);
  store.events.clear();
  void* old_ptr = dst;
  dst = rm.reallocate(dst, 32);
  check_reallocate(operations(store), old_ptr, dst, 32, allocator, false);
  allocator.deallocate(src);
  allocator.deallocate(dst);
}

TEST_F(OperationRecordingTest, ResourceManagerAsyncWrappersRecordOnlyTheirBoundary)
{
  auto& rm = umpire::ResourceManager::getInstance();
  auto allocator = rm.getAllocator("HOST");
  void* src = allocator.allocate(16);
  void* dst = allocator.allocate(16);
  camp::resources::Resource resource{camp::resources::Host{}};
  CapturingStore store;
  RecorderCapture capture(store);
  rm.memset(src, 1, resource, 16);
  rm.copy(dst, src, resource, 16);
  auto events = operations(store);
  ASSERT_EQ(2, events.size());
  for (const auto& event : events)
    EXPECT_EQ("true", event.tags.at("async"));
  store.events.clear();
  void* old_ptr = dst;
  dst = rm.reallocate(dst, 32, resource);
  check_reallocate(operations(store), old_ptr, dst, 32, allocator, true);
  allocator.deallocate(src);
  allocator.deallocate(dst);
}

TEST_F(OperationRecordingTest, ResourceManagerAllocatorReallocateRecordsOnlyItsBoundary)
{
  auto& rm = umpire::ResourceManager::getInstance();
  auto allocator = rm.getAllocator("HOST");
  void* ptr = allocator.allocate(16);
  CapturingStore store;
  RecorderCapture capture(store);
  void* old_ptr = ptr;
  ptr = rm.reallocate(ptr, 32, allocator);
  check_reallocate(operations(store), old_ptr, ptr, 32, allocator, false);
  store.events.clear();
  camp::resources::Resource resource{camp::resources::Host{}};
  old_ptr = ptr;
  ptr = rm.reallocate(ptr, 64, allocator, resource);
  check_reallocate(operations(store), old_ptr, ptr, 64, allocator, true);
  allocator.deallocate(ptr);
}

TEST_F(OperationRecordingTest, ResourceManagerMoveDoesNotRecordInternalCopy)
{
  auto& rm = umpire::ResourceManager::getInstance();
  auto allocator = rm.getAllocator("HOST");
  auto destination = rm.makeAllocator<umpire::strategy::NamedAllocationStrategy>("OPERATION_RECORDING_MOVE", allocator);
  void* ptr = allocator.allocate(16);
  CapturingStore store;
  RecorderCapture capture(store);
  ptr = rm.move(ptr, destination);
  auto events = operations(store);
  ASSERT_EQ(1, events.size());
  EXPECT_EQ("move", events[0].name);
  destination.deallocate(ptr);
  rm.destroyAllocator("OPERATION_RECORDING_MOVE");
}

TEST_F(OperationRecordingTest, ExplicitPlatformsDoNotRecordOperations)
{
  int src[4]{};
  int dst[4]{};
  auto platform = camp::resources::Platform::host;
  camp::resources::Resource resource{camp::resources::Host{}};
  auto allocator = umpire::ResourceManager::getInstance().getAllocator("HOST");
  void* ptr = allocator.allocate(16);
  CapturingStore store;
  RecorderCapture capture(store);
  umpire::memset(platform, src, 1, 4);
  umpire::copy(platform, platform, src, dst, 4);
  umpire::memset(platform, src, 1, 4, resource);
  umpire::copy(platform, platform, src, dst, 4, resource);
  umpire::reallocate(platform, &ptr, 32);
  umpire::reallocate(platform, &ptr, 64, resource);
  EXPECT_TRUE(operations(store).empty());
  allocator.deallocate(ptr);
}

} // namespace
