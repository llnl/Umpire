##############################################################################
# Copyright (c) 2016-25, Lawrence Livermore National Security, LLC and Umpire
# project contributors. See the COPYRIGHT file for details.
#
# SPDX-License-Identifier: (MIT)
##############################################################################
set(test_sources
  copy_test.cpp
  memset_test.cpp
  device_memset_test.cpp
  reallocate_test.cpp
  prefetch_test.cpp
  advise_test.cpp
  sycl_ops_test.cpp
  openmp_target_ops_test.cpp)

blt_add_executable(
  NAME op_tests
  SOURCES ${test_sources}
  DEPENDS_ON umpire gtest)

blt_add_test(
  NAME op_tests
  COMMAND op_tests)
