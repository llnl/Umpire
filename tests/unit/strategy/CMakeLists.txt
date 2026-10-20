##############################################################################
# Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
# project contributors. See the COPYRIGHT file for details.
#
# SPDX-License-Identifier: (MIT)
##############################################################################
set (strategy_tests_depends
  umpire gtest)

if (UMPIRE_ENABLE_OPENMP_TARGET)
  set (strategy_tests_depends
    ${strategy_tests_depends}
    openmp)
endif ()

blt_add_executable(
  NAME pool_heuristics_tests
  SOURCES pool_heuristics_tests.cpp
  DEPENDS_ON ${strategy_tests_depends})

blt_add_test(
  NAME pool_heuristics_tests
  COMMAND pool_heuristics_tests)

blt_add_executable(
  NAME fixed_pool_tests
  SOURCES fixed_pool_tests.cpp
  DEPENDS_ON ${strategy_tests_depends})

blt_add_test(
  NAME fixed_pool_tests
  COMMAND fixed_pool_tests)

blt_add_executable(
  NAME zero_byte_handler_tests
  SOURCES zero_byte_handler_tests.cpp
  DEPENDS_ON ${strategy_tests_depends})

blt_add_test(
  NAME zero_byte_handler_tests
  COMMAND zero_byte_handler_tests)

blt_add_executable(
  NAME aligned_allocation_tests
  SOURCES aligned_allocation_tests.cpp
  DEPENDS_ON ${strategy_tests_depends})

blt_add_test(
  NAME aligned_allocation_tests
  COMMAND aligned_allocation_tests)
