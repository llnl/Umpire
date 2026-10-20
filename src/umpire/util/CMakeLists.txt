##############################################################################
# Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
# project contributors. See the COPYRIGHT file for details.
#
# SPDX-License-Identifier: (MIT)
##############################################################################

set(UMPIRE_ENABLE_SYCL ${UMPIRE_ENABLE_SYCL})

set (umpire_util_headers
  AllocationMap.hpp
  AllocationRecord.hpp
  affinity.hpp
  backtrace.hpp
  backtrace.inl
  error.hpp
  find_first_set.hpp
  FixedMallocPool.hpp
  io.hpp
  Logger.hpp
  MPI.hpp
  Macros.hpp
  MemoryResourceTraits.hpp
  MemoryMap.hpp
  MemoryMap.inl
  node_memory.hpp
  OutputBuffer.hpp
  Platform.hpp
  allocation_statistics.hpp
  detect_vendor.hpp
  make_unique.hpp
  memory_sanitizers.hpp
  shared_memory_helper.hpp
  wrap_allocator.hpp)

if (UMPIRE_ENABLE_NUMA)
  set (umpire_util_headers
    ${umpire_util_headers}
    numa.hpp)
endif ()

set (umpire_util_sources
  AllocationMap.cpp
  affinity.cpp
  FixedMallocPool.cpp
  io.cpp
  Logger.cpp
  MPI.cpp
  node_memory.cpp
  OutputBuffer.cpp
  allocation_statistics.cpp
  detect_vendor.cpp)

if (UMPIRE_ENABLE_NUMA)
  set (umpire_util_sources
    ${umpire_util_sources}
    numa.cpp)
endif ()

set (umpire_util_depends camp umpire_event umpire_tpl_judy ${UMPIRE_FMT_TARGET})

if (NOT "${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
  set (umpire_util_depends
    ${umpire_util_depends}
    backtrace_symbols)
endif ()

if (UMPIRE_ENABLE_NUMA)
  set (umpire_util_depends
    ${umpire_util_depends}
    numa)
endif ()

if (UMPIRE_ENABLE_SLIC AND UMPIRE_ENABLE_LOGGING)
  set (umpire_util_depends
    ${umpire_util_depends}
    slic)
endif()

if (UMPIRE_ENABLE_MPI)
  set (umpire_util_depends
    ${umpire_util_depends}
    mpi)
endif ()

if (UMPIRE_ENABLE_OPENMP_TARGET)
  set (umpire_util_depends
    ${umpire_util_depends}
    openmp)
endif ()

if (UMPIRE_ENABLE_HIP)
  set (umpire_util_depends
    ${umpire_util_depends}
    blt::hip_runtime)
endif ()

if (UMPIRE_ENABLE_CUDA)
  set (umpire_util_depends
    ${umpire_util_depends}
    cuda_runtime)
endif ()


blt_add_library(
  NAME umpire_util
  HEADERS ${umpire_util_headers}
  SOURCES ${umpire_util_sources}
  DEPENDS_ON ${umpire_util_depends}
  OBJECT TRUE)

target_include_directories(
  umpire_util
  PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>
  $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

install(FILES
  ${umpire_util_headers}
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/umpire/util)

umpire_add_code_checks(PREFIX util)
