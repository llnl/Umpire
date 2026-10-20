##############################################################################
# Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
# project contributors. See the COPYRIGHT file for details.
#
# SPDX-License-Identifier: (MIT)
##############################################################################
if (UMPIRE_ENABLE_C)
  set (interface_tests_depends
    umpire gtest)

  if (UMPIRE_ENABLE_CUDA)
    set (interface_tests_depends
      ${interface_tests_depends}
      cuda_runtime)
  endif ()

  if (UMPIRE_ENABLE_OPENMP OR UMPIRE_ENABLE_OPENMP_TARGET)
    set (interface_tests_depends
      ${interface_tests_depends}
      openmp)
  endif ()

  blt_add_executable(
    NAME allocator_c_tests
    SOURCES allocator_c_tests.cpp
    DEPENDS_ON ${interface_tests_depends})

  target_include_directories(
    allocator_c_tests
    PRIVATE
    ${PROJECT_BINARY_DIR}/include)

  blt_add_test(
    NAME allocator_c_tests
    COMMAND allocator_c_tests)

  blt_add_executable(
    NAME operation_c_tests
    SOURCES operation_c_tests.cpp
    DEPENDS_ON ${interface_tests_depends})

  target_include_directories(
    operation_c_tests
    PRIVATE
    ${PROJECT_BINARY_DIR}/include)

  blt_add_test(
    NAME operation_c_tests
    COMMAND operation_c_tests)

  blt_add_executable(
    NAME version_c_tests
    SOURCES version_c_tests.cpp
    DEPENDS_ON ${interface_tests_depends})

  target_include_directories(
    version_c_tests
    PRIVATE
    ${PROJECT_BINARY_DIR}/include)

  blt_add_test(
    NAME version_c_tests
    COMMAND version_c_tests)

  blt_add_executable(
    NAME aligned_pool_c_tests
    SOURCES aligned_pool_c_tests.cpp
    DEPENDS_ON ${interface_tests_depends})

  target_include_directories(
    aligned_pool_c_tests
    PRIVATE
    ${PROJECT_BINARY_DIR}/include)

  blt_add_test(
    NAME aligned_pool_c_tests
    COMMAND aligned_pool_c_tests)

  if (UMPIRE_ENABLE_FORTRAN)
    add_subdirectory(fortran)
  endif()
endif()
