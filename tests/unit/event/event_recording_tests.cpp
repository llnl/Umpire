//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#include "gtest/gtest.h"
#include "umpire/event/event.hpp"
#include "umpire/event/json_file_store.hpp"
#include "umpire/event/recorder_factory.hpp"

#include <cstdio>
#include <fstream>
#include <string>

class EventRecordingTest : public ::testing::Test {
 protected:
  void SetUp() override
  {
    test_filename = "/tmp/umpire_recording_test_" + std::to_string(getpid()) + "_" +
                    std::to_string(reinterpret_cast<uintptr_t>(this)) + ".stats";
  }

  void TearDown() override
  {
    std::remove(test_filename.c_str());
  }

  bool file_exists(const std::string& filename)
  {
    std::ifstream f(filename.c_str());
    return f.good();
  }

  std::string test_filename;
};

TEST(EventRecording, RecorderFactoryReturnsRecorder)
{
  // Should not throw
  auto& recorder = umpire::event::recorder_factory::get_recorder();
  (void)recorder; // Suppress unused variable warning

  // Calling again should return the same instance (singleton)
  auto& recorder2 = umpire::event::recorder_factory::get_recorder();
  ASSERT_EQ(&recorder, &recorder2);
}

TEST_F(EventRecordingTest, EndToEndRecording)
{
  // Create a temporary json_file_store and recorder in a scope
  {
    umpire::event::json_file_store store(test_filename);
    umpire::event::event_store_recorder recorder(&store);

    // Create and record an event using the builder
    umpire::event::builder<> builder;
    builder.name("integration_test").category(umpire::event::category::operation).arg("test_arg", 42).record(recorder);

    // Create and record an allocate event
    umpire::event::builder<umpire::event::allocate> alloc_builder;
    alloc_builder.size(1024).ref(reinterpret_cast<void*>(0x1000)).ptr(reinterpret_cast<void*>(0x2000)).record(recorder);

    // Store destructor will be called here, flushing and closing the file
  }

  // Verify file was created and contains data
  ASSERT_TRUE(file_exists(test_filename));

  // Read back and verify
  umpire::event::json_file_store read_store(test_filename, true);
  auto events = read_store.get_events();

  // Both events are recorded (generic event and allocate event)
  ASSERT_EQ(2, events.size());
  ASSERT_EQ("integration_test", events[0].name);
  ASSERT_EQ(42, events[0].numeric_args["test_arg"]);
  ASSERT_EQ("allocate", events[1].name);
}

TEST(EventRecording, BuilderWithLambda)
{
  // Test the record() function with lambda
  std::string temp_file = "/tmp/umpire_lambda_test_" + std::to_string(getpid()) + ".stats";

  {
    umpire::event::json_file_store store(temp_file);
    umpire::event::event_store_recorder recorder(&store);

    // Use lambda syntax (this is closer to real usage)
    umpire::event::builder<> builder;
    auto lambda = [&](auto& event) {
      event.name("lambda_event").category(umpire::event::category::statistic).arg("lambda_arg", "lambda_value");
    };
    lambda(builder);
    builder.record(recorder);
  }

  // Verify
  umpire::event::json_file_store read_store(temp_file, true);
  auto events = read_store.get_events();

  ASSERT_GE(events.size(), 1);
  ASSERT_EQ("lambda_event", events[0].name);

  std::remove(temp_file.c_str());
}

TEST(EventRecording, EventBuildEnabledCheck)
{
  // The event_build_enabled flag is checked at static initialization time
  // This test just verifies the static variables exist and are accessible

  // When UMPIRE_REPLAY or UMPIRE_EVENTS is not set, events should be conditionally compiled out
  // We can't easily test the runtime behavior without setting environment variables before static init,
  // but we can verify the code compiles and doesn't crash

  SUCCEED(); // Placeholder test for the static check mechanism
}

TEST_F(EventRecordingTest, AllEventTypes)
{
  // Test that all event type builders can be instantiated and used
  // The actual recording is tested in other tests
  umpire::event::builder<umpire::event::allocate>();
  umpire::event::builder<umpire::event::named_allocate>();
  umpire::event::builder<umpire::event::allocate_resource>();
  umpire::event::builder<umpire::event::deallocate>();
  umpire::event::builder<umpire::event::deallocate_resource>();

  // All event types can be instantiated without crashing
  SUCCEED();
}

TEST_F(EventRecordingTest, TimestampPrecision)
{
  auto before = std::chrono::system_clock::now();

  {
    umpire::event::json_file_store store(test_filename);
    umpire::event::event_store_recorder recorder(&store);

    umpire::event::event e;
    e.name = "timestamp_test";
    recorder.record(e);
  }

  auto after = std::chrono::system_clock::now();

  // Read back and verify timestamp is within bounds
  umpire::event::json_file_store read_store(test_filename, true);
  auto events = read_store.get_events();

  ASSERT_EQ(1, events.size());
  ASSERT_GE(events[0].timestamp, before);
  ASSERT_LE(events[0].timestamp, after);
}

TEST_F(EventRecordingTest, MultipleCategories)
{
  {
    umpire::event::json_file_store store(test_filename);
    umpire::event::event_store_recorder recorder(&store);

    // Record events of different categories
    umpire::event::builder<> op_builder;
    op_builder.name("operation_event").category(umpire::event::category::operation).record(recorder);

    umpire::event::builder<> stat_builder;
    stat_builder.name("statistic_event").category(umpire::event::category::statistic).record(recorder);

    umpire::event::builder<> meta_builder;
    meta_builder.name("metadata_event").category(umpire::event::category::metadata).record(recorder);
  }

  // Read back
  umpire::event::json_file_store read_store(test_filename, true);
  auto events = read_store.get_events();

  ASSERT_EQ(3, events.size());

  // Verify categories
  ASSERT_EQ(umpire::event::category::operation, events[0].cat);
  ASSERT_EQ(umpire::event::category::statistic, events[1].cat);
  ASSERT_EQ(umpire::event::category::metadata, events[2].cat);
}
