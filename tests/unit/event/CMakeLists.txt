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
