//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#include "gtest/gtest.h"
#include "umpire/event/event.hpp"
#include "umpire/event/event_store.hpp"
#include "umpire/event/event_store_recorder.hpp"

// Mock event store for testing
class MockEventStore : public umpire::event::event_store {
 public:
  umpire::event::event last_event;
  umpire::event::allocate last_allocate;
  umpire::event::named_allocate last_named_allocate;
  umpire::event::allocate_resource last_allocate_resource;
  umpire::event::deallocate last_deallocate;
  umpire::event::deallocate_resource last_deallocate_resource;

  int event_count = 0;
  int allocate_count = 0;
  int named_allocate_count = 0;
  int allocate_resource_count = 0;
  int deallocate_count = 0;
  int deallocate_resource_count = 0;

  void insert(const umpire::event::event& e) override
  {
    last_event = e;
    event_count++;
  }

  void insert(const umpire::event::allocate& e) override
  {
    last_allocate = e;
    allocate_count++;
  }

  void insert(const umpire::event::named_allocate& e) override
  {
    last_named_allocate = e;
    named_allocate_count++;
  }

  void insert(const umpire::event::allocate_resource& e) override
  {
    last_allocate_resource = e;
    allocate_resource_count++;
  }

  void insert(const umpire::event::deallocate& e) override
  {
    last_deallocate = e;
    deallocate_count++;
  }

  void insert(const umpire::event::deallocate_resource& e) override
  {
    last_deallocate_resource = e;
    deallocate_resource_count++;
  }

  std::vector<umpire::event::event> get_events() override
  {
    return std::vector<umpire::event::event>();
  }
};

TEST(EventStoreRecorder, RecordEvent)
{
  MockEventStore store;
  umpire::event::event_store_recorder recorder(&store);

  umpire::event::event e;
  e.name = "test_event";
  e.cat = umpire::event::category::operation;
  e.string_args["key"] = "value";
  e.numeric_args["num"] = 42;

  recorder.record(e);

  ASSERT_EQ(1, store.event_count);
  ASSERT_EQ("test_event", store.last_event.name);
  ASSERT_EQ(umpire::event::category::operation, store.last_event.cat);
  ASSERT_EQ("value", store.last_event.string_args["key"]);
  ASSERT_EQ(42, store.last_event.numeric_args["num"]);
}

TEST(EventStoreRecorder, RecordAllocate)
{
  MockEventStore store;
  umpire::event::event_store_recorder recorder(&store);

  umpire::event::allocate e;
  e.size = 1024;
  e.ref = reinterpret_cast<void*>(0x1000);
  e.ptr = reinterpret_cast<void*>(0x2000);

  recorder.record(e);

  ASSERT_EQ(1, store.allocate_count);
  ASSERT_EQ(1024, store.last_allocate.size);
  ASSERT_EQ(reinterpret_cast<void*>(0x1000), store.last_allocate.ref);
  ASSERT_EQ(reinterpret_cast<void*>(0x2000), store.last_allocate.ptr);
}

TEST(EventStoreRecorder, RecordNamedAllocate)
{
  MockEventStore store;
  umpire::event::event_store_recorder recorder(&store);

  umpire::event::named_allocate e;
  e.name = "my_allocation";
  e.size = 2048;
  e.ref = reinterpret_cast<void*>(0x1000);
  e.ptr = reinterpret_cast<void*>(0x2000);

  recorder.record(e);

  ASSERT_EQ(1, store.named_allocate_count);
  ASSERT_EQ("my_allocation", store.last_named_allocate.name);
  ASSERT_EQ(2048, store.last_named_allocate.size);
  ASSERT_EQ(reinterpret_cast<void*>(0x1000), store.last_named_allocate.ref);
  ASSERT_EQ(reinterpret_cast<void*>(0x2000), store.last_named_allocate.ptr);
}

TEST(EventStoreRecorder, RecordAllocateResource)
{
  MockEventStore store;
  umpire::event::event_store_recorder recorder(&store);

  umpire::event::allocate_resource e;
  e.size = 4096;
  e.ref = reinterpret_cast<void*>(0x1000);
  e.ptr = reinterpret_cast<void*>(0x2000);
  e.res = "HOST";

  recorder.record(e);

  ASSERT_EQ(1, store.allocate_resource_count);
  ASSERT_EQ(4096, store.last_allocate_resource.size);
  ASSERT_EQ(reinterpret_cast<void*>(0x1000), store.last_allocate_resource.ref);
  ASSERT_EQ(reinterpret_cast<void*>(0x2000), store.last_allocate_resource.ptr);
  ASSERT_EQ("HOST", store.last_allocate_resource.res);
}

TEST(EventStoreRecorder, RecordDeallocate)
{
  MockEventStore store;
  umpire::event::event_store_recorder recorder(&store);

  umpire::event::deallocate e;
  e.ref = reinterpret_cast<void*>(0x1000);
  e.ptr = reinterpret_cast<void*>(0x2000);

  recorder.record(e);

  ASSERT_EQ(1, store.deallocate_count);
  ASSERT_EQ(reinterpret_cast<void*>(0x1000), store.last_deallocate.ref);
  ASSERT_EQ(reinterpret_cast<void*>(0x2000), store.last_deallocate.ptr);
}

TEST(EventStoreRecorder, RecordDeallocateResource)
{
  MockEventStore store;
  umpire::event::event_store_recorder recorder(&store);

  umpire::event::deallocate_resource e;
  e.ref = reinterpret_cast<void*>(0x1000);
  e.ptr = reinterpret_cast<void*>(0x2000);
  e.res = "DEVICE";

  recorder.record(e);

  ASSERT_EQ(1, store.deallocate_resource_count);
  ASSERT_EQ(reinterpret_cast<void*>(0x1000), store.last_deallocate_resource.ref);
  ASSERT_EQ(reinterpret_cast<void*>(0x2000), store.last_deallocate_resource.ptr);
  ASSERT_EQ("DEVICE", store.last_deallocate_resource.res);
}

TEST(EventStoreRecorder, MultipleRecords)
{
  MockEventStore store;
  umpire::event::event_store_recorder recorder(&store);

  // Record different types of events
  umpire::event::event e1;
  e1.name = "event1";
  recorder.record(e1);

  umpire::event::allocate e2;
  e2.size = 1024;
  recorder.record(e2);

  umpire::event::deallocate e3;
  e3.ptr = reinterpret_cast<void*>(0x1000);
  recorder.record(e3);

  ASSERT_EQ(1, store.event_count);
  ASSERT_EQ(1, store.allocate_count);
  ASSERT_EQ(1, store.deallocate_count);
}

TEST(EventStoreRecorder, DataPassedUnchanged)
{
  MockEventStore store;
  umpire::event::event_store_recorder recorder(&store);

  // Create event with specific data
  umpire::event::event original;
  original.name = "precise_event";
  original.cat = umpire::event::category::metadata;
  original.string_args["str1"] = "value1";
  original.string_args["str2"] = "value2";
  original.numeric_args["num1"] = 123;
  original.numeric_args["num2"] = 456;
  original.tags["tag1"] = "tag_value1";

  recorder.record(original);

  // Verify exact data was passed through
  ASSERT_EQ(original.name, store.last_event.name);
  ASSERT_EQ(original.cat, store.last_event.cat);
  ASSERT_EQ(original.string_args.size(), store.last_event.string_args.size());
  ASSERT_EQ(original.numeric_args.size(), store.last_event.numeric_args.size());
  ASSERT_EQ(original.tags.size(), store.last_event.tags.size());

  for (const auto& pair : original.string_args) {
    ASSERT_EQ(pair.second, store.last_event.string_args[pair.first]);
  }
  for (const auto& pair : original.numeric_args) {
    ASSERT_EQ(pair.second, store.last_event.numeric_args[pair.first]);
  }
  for (const auto& pair : original.tags) {
    ASSERT_EQ(pair.second, store.last_event.tags[pair.first]);
  }
}
