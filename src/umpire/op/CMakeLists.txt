##############################################################################
# Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
# project contributors. See the COPYRIGHT file for details.
#
# SPDX-License-Identifier: (MIT)
##############################################################################
set (umpire_op_headers
  GenericReallocateOperation.hpp
  HostCopyOperation.hpp
  HostMemsetOperation.hpp
  HostReallocateOperation.hpp
  MemoryOperation.hpp
  MemoryOperationRegistry.hpp)

set (umpire_op_sources
  GenericReallocateOperation.cpp
  HostCopyOperation.cpp
  HostMemsetOperation.cpp
  HostReallocateOperation.cpp
  MemoryOperation.cpp
  MemoryOperationRegistry.cpp)

set (umpire_op_depends camp umpire_util)

if (UMPIRE_ENABLE_NUMA)
  set (umpire_op_headers
    ${umpire_op_headers}
    NumaMoveOperation.hpp)

  set (umpire_op_sources
    ${umpire_op_sources}
    NumaMoveOperation.cpp)

  set (umpire_op_depends
    ${umpire_op_depends}
    umpire_strategy)
endif ()

if (UMPIRE_ENABLE_CUDA)
  set (umpire_op_headers
    ${umpire_op_headers}
    CudaAdviseOperation.hpp
    CudaCopyOperation.hpp
    CudaMemsetOperation.hpp
    CudaMemPrefetchOperation.hpp)

  set (umpire_op_sources
    ${umpire_op_sources}
    CudaAdviseOperation.cpp
    CudaCopyOperation.cpp
    CudaMemsetOperation.cpp
    CudaMemPrefetchOperation.cpp
    cuda.cu)

  set_source_files_properties(
    cuda.cu
    PROPERTIES LANGUAGE CUDA)

  set (umpire_op_depends
    ${umpire_op_depends}
    cuda_runtime)
endif ()

if (UMPIRE_ENABLE_HIP)
  set (umpire_op_headers
    ${umpire_op_headers}
    HipAdviseOperation.hpp
    HipCopyOperation.hpp
    HipMemsetOperation.hpp)

  set (umpire_op_sources
    ${umpire_op_sources}
    HipAdviseOperation.cpp
    HipCopyOperation.cpp
    HipMemsetOperation.cpp
    hip.cpp)

  set_source_files_properties(
    hip.cpp
    PROPERTIES LANGUAGE HIP)

  set (umpire_op_depends
    ${umpire_op_depends}
    blt::hip_runtime)
endif ()

if (UMPIRE_ENABLE_SYCL)
  set (umpire_op_headers
    ${umpire_op_headers}
    SyclCopyOperation.hpp
    SyclCopyFromOperation.hpp
    SyclCopyToOperation.hpp
    SyclMemsetOperation.hpp
    SyclMemPrefetchOperation.hpp)

  set (umpire_op_sources
    ${umpire_op_sources}
    SyclCopyOperation.cpp
    SyclCopyFromOperation.cpp
    SyclCopyToOperation.cpp
    SyclMemsetOperation.cpp
    SyclMemPrefetchOperation.cpp
    sycl_device_memset.cpp)
endif ()

if (UMPIRE_ENABLE_OPENMP_TARGET)
  message(STATUS "Adding OpenMP Target operations")
  set (umpire_op_headers
    ${umpire_op_headers}
    OpenMPTargetCopyOperation.hpp
    OpenMPTargetMemsetOperation.hpp)

  set (umpire_op_sources
    ${umpire_op_sources}
    OpenMPTargetCopyOperation.cpp
    OpenMPTargetMemsetOperation.cpp)

  set (umpire_op_depends
    ${umpire_op_depends}
    openmp)
endif ()

blt_add_library(
  NAME umpire_op
  HEADERS ${umpire_op_headers}
  SOURCES ${umpire_op_sources}
  DEPENDS_ON ${umpire_op_depends}
  OBJECT TRUE)

target_include_directories(
  umpire_op
  PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

install(FILES
  ${umpire_op_headers}
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/umpire/op)

umpire_add_code_checks(PREFIX op)
