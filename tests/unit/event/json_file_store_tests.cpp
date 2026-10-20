//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#include "gtest/gtest.h"
#include "umpire/event/event.hpp"
#include "umpire/event/json_file_store.hpp"
#include "umpire/util/error.hpp"

#include <cstdio>
#include <fstream>
#include <string>

class JsonFileStoreTest : public ::testing::Test {
 protected:
  void SetUp() override
  {
    // Create unique temporary filename for each test
    test_filename = "/tmp/umpire_test_" + std::to_string(getpid()) + "_" +
                    std::to_string(reinterpret_cast<uintptr_t>(this)) + ".stats";
  }

  void TearDown() override
  {
    // Clean up test file
    std::remove(test_filename.c_str());
  }

  bool file_exists(const std::string& filename)
  {
    std::ifstream f(filename.c_str());
    return f.good();
  }

  std::string read_file_contents(const std::string& filename)
  {
    std::ifstream f(filename);
    std::string contents((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return contents;
  }

  int count_lines(const std::string& filename)
  {
    std::ifstream f(filename);
    int count = 0;
    std::string line;
    while (std::getline(f, line)) {
      count++;
    }
    return count;
  }

  std::string test_filename;
};

TEST_F(JsonFileStoreTest, FileCreation)
{
  // File should not exist before store is created.
  ASSERT_FALSE(file_exists(test_filename));

  {
    umpire::event::json_file_store store(test_filename);
    ASSERT_TRUE(file_exists(test_filename));

    // Create a simple event and insert it.
    umpire::event::event e;
    e.name = "test_event";
    e.cat = umpire::event::category::operation;

    store.insert(e);
  }

  // File should still exist after store is destroyed.
  ASSERT_TRUE(file_exists(test_filename));
}

TEST_F(JsonFileStoreTest, InsertEvent)
{
  {
    umpire::event::json_file_store store(test_filename);

    umpire::event::event e;
    e.name = "test_event";
    e.cat = umpire::event::category::metadata;
    e.string_args["key1"] = "value1";
    e.numeric_args["key2"] = 42;
    e.tags["tag1"] = "tagvalue";

    store.insert(e);
  }

  // Verify file contains the event data
  std::string contents = read_file_contents(test_filename);
  ASSERT_FALSE(contents.empty());
  ASSERT_NE(std::string::npos, contents.find("test_event"));
  ASSERT_NE(std::string::npos, contents.find("metadata"));
  ASSERT_NE(std::string::npos, contents.find("value1"));
}

TEST_F(JsonFileStoreTest, InsertAllocate)
{
  {
    umpire::event::json_file_store store(test_filename);

    umpire::event::allocate e;
    e.size = 1024;
    e.ref = reinterpret_cast<void*>(0x1000);
    e.ptr = reinterpret_cast<void*>(0x2000);

    store.insert(e);
  }

  std::string contents = read_file_contents(test_filename);
  ASSERT_NE(std::string::npos, contents.find("allocate"));
  ASSERT_NE(std::string::npos, contents.find("1024"));
  ASSERT_NE(std::string::npos, contents.find("\"replay\":\"true\""));
}

TEST_F(JsonFileStoreTest, InsertNamedAllocate)
{
  {
    umpire::event::json_file_store store(test_filename);

    umpire::event::named_allocate e;
    e.name = "my_allocation";
    e.size = 2048;
    e.ref = reinterpret_cast<void*>(0x1000);
    e.ptr = reinterpret_cast<void*>(0x2000);

    store.insert(e);
  }

  std::string contents = read_file_contents(test_filename);
  ASSERT_NE(std::string::npos, contents.find("named_allocate"));
  ASSERT_NE(std::string::npos, contents.find("my_allocation"));
  ASSERT_NE(std::string::npos, contents.find("2048"));
}

TEST_F(JsonFileStoreTest, InsertAllocateResource)
{
  {
    umpire::event::json_file_store store(test_filename);

    umpire::event::allocate_resource e;
    e.size = 4096;
    e.ref = reinterpret_cast<void*>(0x1000);
    e.ptr = reinterpret_cast<void*>(0x2000);
    e.res = "HOST";

    store.insert(e);
  }

  std::string contents = read_file_contents(test_filename);
  ASSERT_NE(std::string::npos, contents.find("allocate_resource"));
  ASSERT_NE(std::string::npos, contents.find("HOST"));
  ASSERT_NE(std::string::npos, contents.find("4096"));
}

TEST_F(JsonFileStoreTest, InsertDeallocate)
{
  {
    umpire::event::json_file_store store(test_filename);

    umpire::event::deallocate e;
    e.ref = reinterpret_cast<void*>(0x1000);
    e.ptr = reinterpret_cast<void*>(0x2000);

    store.insert(e);
  }

  std::string contents = read_file_contents(test_filename);
  ASSERT_NE(std::string::npos, contents.find("deallocate"));
}

TEST_F(JsonFileStoreTest, InsertDeallocateResource)
{
  {
    umpire::event::json_file_store store(test_filename);

    umpire::event::deallocate_resource e;
    e.ref = reinterpret_cast<void*>(0x1000);
    e.ptr = reinterpret_cast<void*>(0x2000);
    e.res = "DEVICE";

    store.insert(e);
  }

  std::string contents = read_file_contents(test_filename);
  ASSERT_NE(std::string::npos, contents.find("deallocate_resource"));
  ASSERT_NE(std::string::npos, contents.find("DEVICE"));
}

TEST_F(JsonFileStoreTest, MultipleInserts)
{
  {
    umpire::event::json_file_store store(test_filename);

    // Insert multiple events
    for (int i = 0; i < 5; i++) {
      umpire::event::event e;
      e.name = "event_" + std::to_string(i);
      e.cat = umpire::event::category::statistic;
      store.insert(e);
    }
  }

  // Should have 5 lines (one per event)
  ASSERT_EQ(5, count_lines(test_filename));
}

TEST_F(JsonFileStoreTest, GetEvents)
{
  // Write events to file
  {
    umpire::event::json_file_store store(test_filename);

    umpire::event::event e1;
    e1.name = "event1";
    e1.cat = umpire::event::category::operation;
    e1.string_args["key1"] = "value1";
    e1.numeric_args["num1"] = 100;
    e1.tags["tag1"] = "tagvalue1";

    umpire::event::event e2;
    e2.name = "event2";
    e2.cat = umpire::event::category::metadata;
    e2.string_args["key2"] = "value2";
    e2.numeric_args["num2"] = 200;
    e2.tags["tag2"] = "tagvalue2";

    store.insert(e1);
    store.insert(e2);
  }

  // Read events back
  {
    umpire::event::json_file_store store(test_filename, true); // read-only
    std::vector<umpire::event::event> events = store.get_events();

    ASSERT_EQ(2, events.size());

    ASSERT_EQ("event1", events[0].name);
    ASSERT_EQ(umpire::event::category::operation, events[0].cat);
    ASSERT_EQ("value1", events[0].string_args["key1"]);
    ASSERT_EQ(100, events[0].numeric_args["num1"]);
    ASSERT_EQ("tagvalue1", events[0].tags["tag1"]);

    ASSERT_EQ("event2", events[1].name);
    ASSERT_EQ(umpire::event::category::metadata, events[1].cat);
    ASSERT_EQ("value2", events[1].string_args["key2"]);
    ASSERT_EQ(200, events[1].numeric_args["num2"]);
    ASSERT_EQ("tagvalue2", events[1].tags["tag2"]);
  }
}

TEST_F(JsonFileStoreTest, DestructorClosesFile)
{
  // Write some data
  {
    umpire::event::json_file_store store(test_filename);

    umpire::event::event e;
    e.name = "test_event";
    store.insert(e);
  }
  // Store destructor should have been called here

  // Should be able to reopen the file (verifies it was closed)
  {
    umpire::event::json_file_store store2(test_filename, true);
    auto events = store2.get_events();
    ASSERT_EQ(1, events.size());
  }

  // File should still exist and be readable
  ASSERT_TRUE(file_exists(test_filename));
  std::string contents = read_file_contents(test_filename);
  ASSERT_FALSE(contents.empty());
}

TEST_F(JsonFileStoreTest, ReadOnlyMode)
{
  // First, create a file with some data
  {
    umpire::event::json_file_store store(test_filename);
    umpire::event::event e;
    e.name = "existing_event";
    store.insert(e);
  }

  // Open in read-only mode
  {
    umpire::event::json_file_store readonly_store(test_filename, true);
    auto events = readonly_store.get_events();
    ASSERT_EQ(1, events.size());
    ASSERT_EQ("existing_event", events[0].name);
  }
}

TEST_F(JsonFileStoreTest, ReadOnlyOpenRequiresExistingFile)
{
  ASSERT_FALSE(file_exists(test_filename));
  ASSERT_THROW(umpire::event::json_file_store store(test_filename, true), umpire::runtime_error);
}
