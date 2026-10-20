##############################################################################
# Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
# project contributors. See the COPYRIGHT file for details.
#
# SPDX-License-Identifier: (MIT)
##############################################################################
blt_add_executable(
  NAME exception_tests
  SOURCES exception_tests.cpp
  DEPENDS_ON umpire gtest)

blt_add_test(
  NAME exception_tests
  COMMAND exception_tests)

set (allocation_map_tests_depends
  umpire gtest)

if (UMPIRE_ENABLE_CUDA)
  set (allocation_map_tests_depends
    ${allocation_map_tests_depends}
    cuda_runtime)
endif ()

if (UMPIRE_ENABLE_OPENMP_TARGET)
  set (allocation_map_tests_depends
    ${allocation_map_tests_depends}
    openmp)
endif ()

blt_add_executable(
  NAME allocation_map_tests
  SOURCES allocation_map_tests.cpp
  DEPENDS_ON ${allocation_map_tests_depends})

blt_add_test(
  NAME allocation_map_tests
  COMMAND allocation_map_tests)

blt_add_executable(
  NAME output_buffer_tests
  SOURCES output_buffer_tests.cpp
  DEPENDS_ON umpire gtest)

blt_add_test(
  NAME output_buffer_tests
  COMMAND output_buffer_tests)

blt_add_executable(
  NAME memory_map_tests
  SOURCES memory_map_tests.cpp
  DEPENDS_ON umpire gtest)

blt_add_test(
  NAME memory_map_tests
  COMMAND memory_map_tests)

blt_add_executable(
  NAME find_first_set_tests
  SOURCES find_first_set_tests.cpp
  DEPENDS_ON umpire gtest)

blt_add_test(
  NAME find_first_set_tests
  COMMAND find_first_set_tests)

blt_add_executable(
  NAME memory_resource_traits_to_string_tests
  SOURCES test_memory_resource_traits_to_string.cpp
  DEPENDS_ON umpire gtest)

blt_add_test(
  NAME memory_resource_traits_to_string_tests
  COMMAND memory_resource_traits_to_string_tests)

blt_add_executable(
  NAME node_memory_tests
  SOURCES node_memory_tests.cpp
  DEPENDS_ON umpire gtest)

blt_add_test(
  NAME node_memory_tests
  COMMAND node_memory_tests)

blt_add_executable(
  NAME fixed_malloc_pool_tests
  SOURCES fixed_malloc_pool_tests.cpp
  DEPENDS_ON umpire gtest)

blt_add_test(
  NAME fixed_malloc_pool_tests
  COMMAND fixed_malloc_pool_tests)
