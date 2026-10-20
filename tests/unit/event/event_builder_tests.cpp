//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#include "gtest/gtest.h"
#include "umpire/event/event.hpp"

#include <memory>

// Mock recorder state (shared)
struct MockRecorderState {
  umpire::event::event last_event;
  umpire::event::allocate last_allocate;
  umpire::event::named_allocate last_named_allocate;
  umpire::event::allocate_resource last_allocate_resource;
  umpire::event::deallocate last_deallocate;
  umpire::event::deallocate_resource last_deallocate_resource;
};

// Mock recorder for testing without file I/O
// Uses shared state pointer so copies still record to same location
class MockRecorder {
 public:
  MockRecorder() : state(new MockRecorderState())
  {
  }

  // Copy constructor shares state
  MockRecorder(const MockRecorder& other) : state(other.state)
  {
  }

  void record(const umpire::event::event& e) const
  {
    state->last_event = e;
  }
  void record(const umpire::event::allocate& e) const
  {
    state->last_allocate = e;
  }
  void record(const umpire::event::named_allocate& e) const
  {
    state->last_named_allocate = e;
  }
  void record(const umpire::event::allocate_resource& e) const
  {
    state->last_allocate_resource = e;
  }
  void record(const umpire::event::deallocate& e) const
  {
    state->last_deallocate = e;
  }
  void record(const umpire::event::deallocate_resource& e) const
  {
    state->last_deallocate_resource = e;
  }

  std::shared_ptr<MockRecorderState> state;
};

TEST(EventBuilder, BasicConstruction)
{
  MockRecorder recorder;
  umpire::event::builder<> builder;

  builder.name("test_event").record(recorder);

  ASSERT_EQ("test_event", recorder.state->last_event.name);
}

TEST(EventBuilder, MethodChaining)
{
  MockRecorder recorder;

  umpire::event::builder<> builder;
  builder.name("chained_event")
      .category(umpire::event::category::operation)
      .arg("key1", "value1")
      .arg("key2", 42)
      .tag("tag1", "tagvalue")
      .record(recorder);

  ASSERT_EQ("chained_event", recorder.state->last_event.name);
  ASSERT_EQ(umpire::event::category::operation, recorder.state->last_event.cat);
  ASSERT_EQ("value1", recorder.state->last_event.string_args["key1"]);
  ASSERT_EQ(42, recorder.state->last_event.numeric_args["key2"]);
  ASSERT_EQ("tagvalue", recorder.state->last_event.tags["tag1"]);
}

TEST(EventBuilder, StringArguments)
{
  MockRecorder recorder;

  umpire::event::builder<> builder;
  builder.name("string_test").arg("str1", std::string("test")).arg("str2", "cstr").record(recorder);

  ASSERT_EQ("test", recorder.state->last_event.string_args["str1"]);
  ASSERT_EQ("cstr", recorder.state->last_event.string_args["str2"]);
}

TEST(EventBuilder, NumericArguments)
{
  MockRecorder recorder;

  umpire::event::builder<> builder;
  builder.name("numeric_test")
      .arg("int_val", 123)
      .arg("size_t_val", static_cast<std::size_t>(456))
      .arg("double_val", 3.14)
      .record(recorder);

  ASSERT_EQ(123, recorder.state->last_event.numeric_args["int_val"]);
  ASSERT_EQ(456, recorder.state->last_event.numeric_args["size_t_val"]);
  ASSERT_EQ(3, recorder.state->last_event.numeric_args["double_val"]); // Truncated to integer
}

TEST(EventBuilder, PointerArguments)
{
  MockRecorder recorder;
  void* test_ptr = reinterpret_cast<void*>(0x12345678);

  umpire::event::builder<> builder;
  builder.name("pointer_test").arg("ptr", test_ptr).record(recorder);

  ASSERT_NE(recorder.state->last_event.string_args.end(), recorder.state->last_event.string_args.find("ptr"));
  ASSERT_FALSE(recorder.state->last_event.string_args["ptr"].empty());
}

TEST(EventBuilder, Tags)
{
  MockRecorder recorder;

  umpire::event::builder<> builder;
  builder.name("tag_test").tag("replay", "true").tag("version", "1.0").record(recorder);

  ASSERT_EQ("true", recorder.state->last_event.tags["replay"]);
  ASSERT_EQ("1.0", recorder.state->last_event.tags["version"]);
}

TEST(EventBuilder, CategoryDefault)
{
  MockRecorder recorder;

  umpire::event::builder<> builder;
  builder.name("default_category").record(recorder);

  ASSERT_EQ(umpire::event::category::statistic, recorder.state->last_event.cat);
}

TEST(EventBuilder, CategoryExplicit)
{
  MockRecorder recorder;

  umpire::event::builder<> builder;
  builder.name("metadata_event").category(umpire::event::category::metadata).record(recorder);

  ASSERT_EQ(umpire::event::category::metadata, recorder.state->last_event.cat);
}

TEST(EventBuilder, AllocateBuilder)
{
  MockRecorder recorder;
  void* test_ref = reinterpret_cast<void*>(0x1000);
  void* test_ptr = reinterpret_cast<void*>(0x2000);

  umpire::event::builder<umpire::event::allocate> builder;
  builder.size(1024).ref(test_ref).ptr(test_ptr).record(recorder);

  ASSERT_EQ(1024, recorder.state->last_allocate.size);
  ASSERT_EQ(test_ref, recorder.state->last_allocate.ref);
  ASSERT_EQ(test_ptr, recorder.state->last_allocate.ptr);
}

TEST(EventBuilder, NamedAllocateBuilder)
{
  MockRecorder recorder;
  void* test_ref = reinterpret_cast<void*>(0x1000);
  void* test_ptr = reinterpret_cast<void*>(0x2000);

  umpire::event::builder<umpire::event::named_allocate> builder;
  builder.name("my_allocation").size(2048).ref(test_ref).ptr(test_ptr).record(recorder);

  ASSERT_EQ("my_allocation", recorder.state->last_named_allocate.name);
  ASSERT_EQ(2048, recorder.state->last_named_allocate.size);
  ASSERT_EQ(test_ref, recorder.state->last_named_allocate.ref);
  ASSERT_EQ(test_ptr, recorder.state->last_named_allocate.ptr);
}

TEST(EventBuilder, AllocateResourceBuilder)
{
  MockRecorder recorder;
  void* test_ref = reinterpret_cast<void*>(0x1000);
  void* test_ptr = reinterpret_cast<void*>(0x2000);

  umpire::event::builder<umpire::event::allocate_resource> builder;
  builder.size(4096).ref(test_ref).ptr(test_ptr).res("HOST").record(recorder);

  ASSERT_EQ(4096, recorder.state->last_allocate_resource.size);
  ASSERT_EQ(test_ref, recorder.state->last_allocate_resource.ref);
  ASSERT_EQ(test_ptr, recorder.state->last_allocate_resource.ptr);
  ASSERT_EQ("HOST", recorder.state->last_allocate_resource.res);
}

TEST(EventBuilder, DeallocateBuilder)
{
  MockRecorder recorder;
  void* test_ref = reinterpret_cast<void*>(0x1000);
  void* test_ptr = reinterpret_cast<void*>(0x2000);

  umpire::event::builder<umpire::event::deallocate> builder;
  builder.ref(test_ref).ptr(test_ptr).record(recorder);

  ASSERT_EQ(test_ref, recorder.state->last_deallocate.ref);
  ASSERT_EQ(test_ptr, recorder.state->last_deallocate.ptr);
}

TEST(EventBuilder, DeallocateResourceBuilder)
{
  MockRecorder recorder;
  void* test_ref = reinterpret_cast<void*>(0x1000);
  void* test_ptr = reinterpret_cast<void*>(0x2000);

  umpire::event::builder<umpire::event::deallocate_resource> builder;
  builder.ref(test_ref).ptr(test_ptr).res("DEVICE").record(recorder);

  ASSERT_EQ(test_ref, recorder.state->last_deallocate_resource.ref);
  ASSERT_EQ(test_ptr, recorder.state->last_deallocate_resource.ptr);
  ASSERT_EQ("DEVICE", recorder.state->last_deallocate_resource.res);
}

TEST(EventBuilder, TimestampIsSet)
{
  MockRecorder recorder;
  auto before = std::chrono::system_clock::now();

  umpire::event::builder<> builder;
  builder.name("timestamp_test").record(recorder);

  auto after = std::chrono::system_clock::now();

  ASSERT_GE(recorder.state->last_event.timestamp, before);
  ASSERT_LE(recorder.state->last_event.timestamp, after);
}
