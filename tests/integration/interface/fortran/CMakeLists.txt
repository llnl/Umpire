##############################################################################
# Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
# project contributors. See the COPYRIGHT file for details.
#
# SPDX-License-Identifier: (MIT)
##############################################################################
set (fortran_test_defines)

set (fortran_test_defines ${fortran_test_defines} -DUMPIRE_VERSION_MAJOR=${Umpire_VERSION_MAJOR})
set (fortran_test_defines ${fortran_test_defines} -DUMPIRE_VERSION_MINOR=${Umpire_VERSION_MINOR})
set (fortran_test_defines ${fortran_test_defines} -DUMPIRE_VERSION_PATCH=${Umpire_VERSION_PATCH})

if (UMPIRE_ENABLE_CUDA)
  set (fortran_test_defines ${fortran_test_defines} -DUMPIRE_ENABLE_CUDA)
endif ()

if (UMPIRE_ENABLE_HIP)
  set (fortran_test_defines ${fortran_test_defines} -DUMPIRE_ENABLE_HIP)
endif ()

set (fortran_tests_depends
  umpire fruit)

if (UMPIRE_ENABLE_CUDA)
  set (fortran_tests_depends
    ${fortran_tests_depends}
    cuda_runtime)
endif ()

if (UMPIRE_ENABLE_OPENMP OR UMPIRE_ENABLE_OPENMP_TARGET)
  set (fortran_tests_depends
    ${fortran_tests_depends}
    openmp)
endif ()

blt_add_executable(
  NAME allocator_fortran_tests
  SOURCES allocator_fortran_tests.F
  DEFINES ${fortran_test_defines}
  DEPENDS_ON ${fortran_tests_depends})

set_source_files_properties(
  allocator_fortran_tests.F
  PROPERTIES
  Fortran_FORMAT FREE)

blt_add_test(
  NAME allocator_fortran_tests
  COMMAND allocator_fortran_tests)

blt_add_executable(
  NAME op_fortran_tests
  SOURCES op_fortran_tests.F
  DEFINES ${fortran_test_defines}
  DEPENDS_ON ${fortran_tests_depends})

set_source_files_properties(
  op_fortran_tests.F
  PROPERTIES
  Fortran_FORMAT FREE)

blt_add_test(
  NAME op_fortran_tests
  COMMAND op_fortran_tests)

blt_add_executable(
  NAME introspection_fortran_tests
  SOURCES introspection_fortran_tests.F
  DEFINES ${fortran_test_defines}
  DEPENDS_ON ${fortran_tests_depends})

set_source_files_properties(
  introspection_fortran_tests.F
  PROPERTIES
  Fortran_FORMAT FREE)

blt_add_test(
  NAME introspection_fortran_tests
  COMMAND introspection_fortran_tests)

blt_add_executable(
  NAME version_fortran_tests
  SOURCES version_fortran_tests.F
  DEFINES ${fortran_test_defines}
  DEPENDS_ON ${fortran_tests_depends})

set_source_files_properties(
  version_fortran_tests.F
  PROPERTIES
  Fortran_FORMAT FREE)

blt_add_test(
  NAME version_fortran_tests
  COMMAND version_fortran_tests)

blt_add_executable(
  NAME aligned_pool_fortran_tests
  SOURCES aligned_pool_fortran_tests.F
  DEFINES ${fortran_test_defines}
  DEPENDS_ON ${fortran_tests_depends})

set_source_files_properties(
  aligned_pool_fortran_tests.F
  PROPERTIES
  Fortran_FORMAT FREE)

blt_add_test(
  NAME aligned_pool_fortran_tests
  COMMAND aligned_pool_fortran_tests)
