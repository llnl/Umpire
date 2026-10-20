##############################################################################
# Copyright (c) 2016-20, Lawrence Livermore National Security, LLC and Umpire
# project contributors. See the COPYRIGHT file for details.
#
# SPDX-License-Identifier: (MIT)
##############################################################################

option(UMPIRE_ENABLE_DEVELOPER_DEFAULTS "Enable default options for Umpire developers" Off)
option(UMPIRE_ENABLE_DEVELOPER_BENCHMARKS "Enable benchmarks for Umpire developers" Off)
mark_as_advanced(UMPIRE_ENABLE_DEVELOPER_DEFAULTS UMPIRE_ENABLE_DEVELOPER_BENCHMARKS)

if (UMPIRE_ENABLE_DEVELOPER_DEFAULTS)
  set(ENABLE_WARNINGS_AS_ERRORS On CACHE BOOL "")
  set(UMPIRE_ENABLE_TOOLS On CACHE BOOL "")
endif ()

if(WIN32 OR APPLE)
  set(UMPIRE_ENABLE_FILE_RESOURCE Off CACHE BOOL "")
endif()
option(UMPIRE_ENABLE_FILE_RESOURCE "Enable File Resource" On)
option(UMPIRE_ENABLE_UMAP "Enable UMAP allocator" Off)

option(UMPIRE_ENABLE_SYCL "Build Umpire with SYCL" Off)
option(UMPIRE_ENABLE_NUMA "Build Umpire with NUMA support" Off)
option(UMPIRE_ENABLE_OPENMP_TARGET "Build Umpire with OPENMP target" Off)

option(UMPIRE_ENABLE_LOGGING "Build Umpire with Logging enabled" On)
option(UMPIRE_ENABLE_SLIC "Build Umpire with SLIC logging" Off)
option(UMPIRE_ENABLE_BACKTRACE "Build Umpire with allocation backtrace enabled" Off)
option(UMPIRE_ENABLE_BACKTRACE_SYMBOLS "Build Umpire with symbol support" Off)
option(UMPIRE_ENABLE_PEDANTIC_WARNINGS "Enable pedantic compiler warnings" On)
option(UMPIRE_ENABLE_INACCESSIBILITY_TESTS "Test allocator inaccessibility functionality" Off)
option(UMPIRE_ENABLE_TOOLS "Enable Umpire development tools" Off)
option(UMPIRE_ENABLE_C "Enable C support within Umpire" Off)
option(UMPIRE_ENABLE_DEVICE_CONST "Enable constant memory on GPUs" Off)
option(UMPIRE_ENABLE_PERFORMANCE_TESTS "Enable additional performance tests" Off)
option(UMPIRE_ENABLE_ASAN "Enable use with address sanitizer tools" Off)
option(UMPIRE_ENABLE_SANITIZER_TESTS "Enable address sanitizer tests" Off)
option(UMPIRE_ENABLE_DEVICE_ALLOCATOR "Enable Device Allocator" Off)
option(UMPIRE_ENABLE_SQLITE_EXPERIMENTAL "Build with sqlite event integration (experimental)" Off)
option(UMPIRE_DISABLE_ALLOCATIONMAP_DEBUG "Disable verbose output from AllocationMap during debug builds" Off)
option(UMPIRE_ENABLE_LEGACY_CMAKE_TARGETS "Enable legacy non-namespaced targets (e.g., umpire) for backwards compatibility" On)
set(UMPIRE_FMT_TARGET fmt::fmt-header-only CACHE STRING "Name of fmt target to use") 

if (UMPIRE_ENABLE_INACCESSIBILITY_TESTS)
  set(ENABLE_GTEST_DEATH_TESTS On CACHE BOOL "Enable tests asserting failure.")
endif()

set(ENABLE_CUDA Off CACHE BOOL "")
set(ENABLE_GMOCK On CACHE BOOL "")
