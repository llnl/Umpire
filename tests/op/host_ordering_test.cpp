//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-25, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////

#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "gtest/gtest.h"
#include "umpire/Allocator.hpp"
#include "umpire/ResourceManager.hpp"
#include "umpire/op.hpp"

namespace host_ordering_test {

// Prior resource work executes only when its event is waited on. This models
// queued transfers that read or write host memory without timing or GPU dependencies.
struct DeferredEvent {
  std::shared_ptr<std::function<void()>> pending;

  camp::resources::Platform get_platform() const
  {
    return camp::resources::Platform::host;
  }
  bool check() const
  {
    return !*pending;
  }
  void wait() const
  {
    if (*pending) {
      auto work = std::move(*pending);
      *pending = {};
      work();
    }
  }
  std::size_t get_hash() const
  {
    return 0;
  }
  bool operator==(const DeferredEvent& other) const
  {
    return pending == other.pending;
  }
};

struct DeferredResource : camp::resources::Host {
  std::shared_ptr<std::function<void()>> pending;

  explicit DeferredResource(std::function<void()> work)
      : pending(std::make_shared<std::function<void()>>(std::move(work)))
  {
  }

  camp::resources::Event get_event_erased();
};

} // namespace host_ordering_test

namespace camp {
namespace resources {
inline namespace v1 {
template <>
struct is_concrete_event_impl<host_ordering_test::DeferredEvent> : std::true_type {};
template <>
struct is_concrete_resource_impl<host_ordering_test::DeferredResource> : std::true_type {};
} // namespace v1
} // namespace resources
} // namespace camp

camp::resources::Event host_ordering_test::DeferredResource::get_event_erased()
{
  return camp::resources::Event{DeferredEvent{pending}};
}

namespace {

enum class DispatchPath { CompileTime, Runtime, PlatformValue, ResourceManager };

// Exercise the compatibility API deliberately, including builds with -Werror.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#elif defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

class HostOrdering : public ::testing::TestWithParam<DispatchPath> {
 protected:
  umpire::Allocator allocator{umpire::ResourceManager::getInstance().getAllocator("HOST")};
  int* src{nullptr};
  int* dst{nullptr};

  void SetUp() override
  {
    src = static_cast<int*>(allocator.allocate(sizeof(int)));
    dst = static_cast<int*>(allocator.allocate(sizeof(int)));
  }

  void TearDown() override
  {
    allocator.deallocate(src);
    allocator.deallocate(dst);
  }

  camp::resources::EventProxy<camp::resources::Resource> copy(camp::resources::Resource& ctx)
  {
    switch (GetParam()) {
      case DispatchPath::CompileTime:
        return umpire::copy<umpire::resource::host_platform, umpire::resource::host_platform>(src, dst, 1, ctx);
      case DispatchPath::Runtime:
        return umpire::copy(src, dst, 1, ctx);
      case DispatchPath::PlatformValue:
        return umpire::copy(camp::resources::Platform::host, camp::resources::Platform::host, src, dst, 1, ctx);
      default:
        return umpire::ResourceManager::getInstance().copy(dst, src, ctx, sizeof(int));
    }
  }

  camp::resources::EventProxy<camp::resources::Resource> memset(camp::resources::Resource& ctx)
  {
    switch (GetParam()) {
      case DispatchPath::CompileTime:
        return umpire::memset<umpire::resource::host_platform>(dst, 0, 1, ctx);
      case DispatchPath::Runtime:
        return umpire::memset(dst, 0, 1, ctx);
      case DispatchPath::PlatformValue:
        return umpire::memset(camp::resources::Platform::host, dst, 0, 1, ctx);
      default:
        return umpire::ResourceManager::getInstance().memset(dst, 0, ctx, sizeof(int));
    }
  }
};

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#endif

TEST_P(HostOrdering, CopyWaitsForPendingSourceWrite)
{
  *src = 0;
  *dst = -1;
  camp::resources::Resource ctx{host_ordering_test::DeferredResource{[this] { *src = 42; }}};

  auto event = copy(ctx);

  EXPECT_EQ(*dst, 42);
  EXPECT_TRUE(static_cast<camp::resources::Event>(event).check());
}

TEST_P(HostOrdering, CopyWaitsForPendingDestinationRead)
{
  *src = 42;
  *dst = 7;
  int observed = -1;
  camp::resources::Resource ctx{host_ordering_test::DeferredResource{[this, &observed] { observed = *dst; }}};

  auto event = copy(ctx);

  EXPECT_EQ(observed, 7);
  EXPECT_EQ(*dst, 42);
  EXPECT_TRUE(static_cast<camp::resources::Event>(event).check());
}

TEST_P(HostOrdering, MemsetWaitsForPendingWrite)
{
  *dst = -1;
  camp::resources::Resource ctx{host_ordering_test::DeferredResource{[this] { *dst = 42; }}};

  auto event = memset(ctx);

  EXPECT_EQ(*dst, 0);
  EXPECT_TRUE(static_cast<camp::resources::Event>(event).check());
  static_cast<camp::resources::Event>(event).wait();
  EXPECT_EQ(*dst, 0);
}

TEST_P(HostOrdering, MemsetWaitsForPendingRead)
{
  *dst = 7;
  int observed = -1;
  camp::resources::Resource ctx{host_ordering_test::DeferredResource{[this, &observed] { observed = *dst; }}};

  auto event = memset(ctx);

  EXPECT_EQ(observed, 7);
  EXPECT_EQ(*dst, 0);
  EXPECT_TRUE(static_cast<camp::resources::Event>(event).check());
}

std::string dispatch_path_name(const ::testing::TestParamInfo<DispatchPath>& info)
{
  const char* names[] = {"CompileTime", "Runtime", "PlatformValue", "ResourceManager"};
  return names[static_cast<int>(info.param)];
}

INSTANTIATE_TEST_SUITE_P(HostAsync, HostOrdering,
                         ::testing::Values(DispatchPath::CompileTime, DispatchPath::Runtime,
                                           DispatchPath::PlatformValue, DispatchPath::ResourceManager),
                         dispatch_path_name);

} // namespace
