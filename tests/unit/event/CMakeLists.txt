##############################################################################
# Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
# project contributors. See the COPYRIGHT file for details.
#
# SPDX-License-Identifier: (MIT)
##############################################################################

# Event builder tests
blt_add_executable(
  NAME event_builder_tests
  SOURCES event_builder_tests.cpp
  DEPENDS_ON umpire gtest)

blt_add_test(
  NAME event_builder_tests
  COMMAND event_builder_tests)

# JSON file store tests
blt_add_executable(
  NAME json_file_store_tests
  SOURCES json_file_store_tests.cpp
  DEPENDS_ON umpire gtest)

blt_add_test(
  NAME json_file_store_tests
  COMMAND json_file_store_tests)

# Event store recorder tests
blt_add_executable(
  NAME event_store_recorder_tests
  SOURCES event_store_recorder_tests.cpp
  DEPENDS_ON umpire gtest)

blt_add_test(
  NAME event_store_recorder_tests
  COMMAND event_store_recorder_tests)

# Integration tests
blt_add_executable(
  NAME event_recording_tests
  SOURCES event_recording_tests.cpp
  DEPENDS_ON umpire gtest)

blt_add_test(
  NAME event_recording_tests
  COMMAND event_recording_tests)

blt_add_executable(
  NAME operation_recording_tests
  SOURCES operation_recording_tests.cpp
  DEPENDS_ON umpire gtest)

blt_add_target_compile_flags(
  TO operation_recording_tests
  FLAGS ${UMPIRE_DISABLE_DEPRECATED_WARNINGS_FLAG})

blt_add_test(
  NAME operation_recording_tests
  COMMAND operation_recording_tests)

set_tests_properties(operation_recording_tests PROPERTIES ENVIRONMENT "UMPIRE_EVENTS=1")
