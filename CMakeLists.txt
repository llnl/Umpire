##############################################################################
# Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
# project contributors. See the COPYRIGHT file for details.
#
# SPDX-License-Identifier: (MIT)
##############################################################################
cmake_minimum_required(VERSION 3.23)

cmake_policy(SET CMP0025 NEW)
cmake_policy(SET CMP0048 NEW)
cmake_policy(SET CMP0057 NEW)
cmake_policy(SET CMP0054 NEW)
cmake_policy(SET CMP0096 OLD)

include(CMakeDependentOption)
include(CMakePackageConfigHelpers)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" VERSION_STRING)
string(STRIP "${VERSION_STRING}" VERSION_STRING)

project(Umpire
  LANGUAGES CXX C
  VERSION ${VERSION_STRING})

set(UMPIRE_VERSION_RC "")

include(cmake/SetupUmpireOptions.cmake)

# Default to C++20 if not specified by the user.
set(BLT_CXX_STD "c++20" CACHE STRING "Version of C++ standard")

# If BLT_CXX_STD is set by the user, check that it is at least 20.
if("${BLT_CXX_STD}" STREQUAL "c++98" OR
   "${BLT_CXX_STD}" STREQUAL "c++03" OR
   "${BLT_CXX_STD}" STREQUAL "c++11" OR
   "${BLT_CXX_STD}" STREQUAL "c++14" OR
   "${BLT_CXX_STD}" STREQUAL "c++17")
  message(FATAL_ERROR "Umpire requires a minimum C++ standard of c++20. Please set BLT_CXX_STD accordingly.")
endif()

# If CMAKE_CUDA_STANDARD is set by the user, check that it is at least 20.
# If it is not set, it will be set later by BLT to match BLT_CXX_STD.
if ("${CMAKE_CUDA_STANDARD}" STREQUAL "98" OR
    "${CMAKE_CUDA_STANDARD}" STREQUAL "03" OR
    "${CMAKE_CUDA_STANDARD}" STREQUAL "11" OR
    "${CMAKE_CUDA_STANDARD}" STREQUAL "14" OR
    "${CMAKE_CUDA_STANDARD}" STREQUAL "17")
  message(FATAL_ERROR "Umpire requires a minimum CUDA standard of 20. Please set BLT_CXX_STD and/or CMAKE_CUDA_STANDARD accordingly.")
endif()

# If CMAKE_HIP_STANDARD is set by the user, check that it is at least 20
# If it is not set, it will be set later by BLT to match BLT_CXX_STD
if ("${CMAKE_HIP_STANDARD}" STREQUAL "98" OR
    "${CMAKE_HIP_STANDARD}" STREQUAL "03" OR
    "${CMAKE_HIP_STANDARD}" STREQUAL "11" OR
    "${CMAKE_HIP_STANDARD}" STREQUAL "14" OR
    "${CMAKE_HIP_STANDARD}" STREQUAL "17")
  message(FATAL_ERROR "Umpire requires a minimum HIP standard of 20. Please set BLT_CXX_STD and/or CMAKE_HIP_STANDARD accordingly.")
endif()

if (UMPIRE_ENABLE_SYCL)
   if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "IntelLLVM")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsycl")
   endif()
endif()

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "PGI")
  message(STATUS "Setting CMAKE_CXX_EXTENSIONS to ON for PGI Compiler")
  set( CMAKE_CXX_EXTENSIONS ON )
endif()

message(STATUS "Using CMake version ${CMAKE_VERSION}")

if (UMPIRE_ENABLE_NUMA AND (NOT UNIX OR APPLE))
  message(FATAL_ERROR "\
  NUMA support unavailable. \
  Please re-configure with UMPIRE_ENABLE_NUMA=Off (default value)")
endif ()

set(BLT_EXPORT_THIRDPARTY OFF CACHE BOOL "")

################################
# BLT
################################
if (NOT BLT_LOADED)
  if (DEFINED BLT_SOURCE_DIR)
    if (NOT EXISTS ${BLT_SOURCE_DIR}/SetupBLT.cmake)
        message(FATAL_ERROR "Given BLT_SOURCE_DIR does not contain SetupBLT.cmake")
    endif()
  else ()
    set (BLT_SOURCE_DIR ${PROJECT_SOURCE_DIR}/blt CACHE PATH "")

    if (NOT EXISTS ${BLT_SOURCE_DIR}/SetupBLT.cmake)
      message(FATAL_ERROR
          "The BLT git submodule is not present. "
          "Either run the following two commands in your git repository: \n"
          "    git submodule init\n"
          "    git submodule update\n"
          "Or add -DBLT_SOURCE_DIR=/path/to/blt to your CMake command." )
    endif ()
  endif ()

  message(STATUS "BLT Source Directory: ${BLT_SOURCE_DIR}")
  include(${BLT_SOURCE_DIR}/SetupBLT.cmake)
endif()

#######################################
# Options that depend on BLT Options
#######################################
cmake_dependent_option( UMPIRE_ENABLE_CUDA "Build Umpire with CUDA support" On
                        "ENABLE_CUDA" Off )
cmake_dependent_option( UMPIRE_ENABLE_HIP "Build Umpire with HIP" On
                       "ENABLE_HIP" Off )
cmake_dependent_option( UMPIRE_ENABLE_OPENMP "Build Umpire with OpenMP" On
                       "ENABLE_OPENMP" Off )
cmake_dependent_option( UMPIRE_ENABLE_MPI "Build Umpire with MPI" On
                       "ENABLE_MPI" Off )

########
# Shared Memory Options
########

cmake_dependent_option( UMPIRE_ENABLE_IPC_SHARED_MEMORY "Enable Host IPC Shared Memory Resource" ${UMPIRE_ENABLE_MPI}
                        "NOT WIN32;NOT APPLE" Off )
cmake_dependent_option( UMPIRE_ENABLE_MPI3_SHARED_MEMORY "Enable MPI3 Shared Memory Resource" Off "${ENABLE_MPI}" Off)

if(UMPIRE_ENABLE_IPC_SHARED_MEMORY AND NOT UMPIRE_ENABLE_MPI3_SHARED_MEMORY)
  set(UMPIRE_DEFAULT_SHARED_MEMORY_RESOURCE "POSIX" CACHE STRING "Default implementation for SHARED memory resource (POSIX)")
elseif(UMPIRE_ENABLE_MPI3_SHARED_MEMORY)
  set(UMPIRE_DEFAULT_SHARED_MEMORY_RESOURCE "MPI3" CACHE STRING "Default implementation for SHARED memory resource (MPI3)")
else()
  set (UMPIRE_DEFAULT_SHARED_MEMORY_RESOURCE "" CACHE STRING "")
endif()

if(UMPIRE_DEFAULT_SHARED_MEMORY_RESOURCE)
  message(STATUS "Default Shared Memory Resource is ${UMPIRE_DEFAULT_SHARED_MEMORY_RESOURCE}")
endif()

########

cmake_dependent_option( UMPIRE_ENABLE_TESTS "Build Umpire tests" On
                       "ENABLE_TESTS" Off )
cmake_dependent_option( UMPIRE_ENABLE_BENCHMARKS "Build Umpire benchmarks" On
                        "ENABLE_BENCHMARKS" Off )
cmake_dependent_option( UMPIRE_ENABLE_EXAMPLES "Build Umpire examples" On
                        "ENABLE_EXAMPLES" Off )

cmake_dependent_option( UMPIRE_ENABLE_DOCS "Build Umpire docs" Off
                        "ENABLE_DOCS" Off )
cmake_dependent_option( UMPIRE_ENABLE_CLANGQUERY "Build Umpire with Clang query" On
                        "ENABLE_CLANGQUERY" Off )
cmake_dependent_option( UMPIRE_ENABLE_COVERAGE "Build Umpire with Coverage support (with GCC)" On
                        "ENABLE_COVERAGE" Off )
cmake_dependent_option( UMPIRE_ENABLE_GMOCK "Build Umpire with gmock" On
                        "ENABLE_GMOCK" Off )
cmake_dependent_option( UMPIRE_ENABLE_FORTRAN "Build Umpire with FORTRAN" On
                        "ENABLE_FORTRAN" Off )

if (UMPIRE_ENABLE_FORTRAN)
  set(UMPIRE_ENABLE_C On)
endif()

if (UMPIRE_ENABLE_DOCS AND NOT ENABLE_DOXYGEN)
  message(FATAL_ERROR "\
  Sphinx documentation requires Doxygen, \
  please re-configure with ENABLE_DOXYGEN=ON")
endif ()

if (Git_FOUND)
  blt_git_hashcode (HASHCODE umpire_sha1
                    RETURN_CODE rc
                    SOURCE_DIR ${PROJECT_SOURCE_DIR})
  set (UMPIRE_VERSION_RC ${umpire_sha1})
endif ()

include(cmake/SetupCMakeBasics.cmake)
include(cmake/SetupCompilerFlags.cmake)
include(cmake/SetupUmpireThirdParty.cmake)

add_subdirectory(src)

configure_package_config_file(
  "${PROJECT_SOURCE_DIR}/umpire-config.cmake.in"
  "${PROJECT_BINARY_DIR}/umpire-config.cmake"
  PATH_VARS CMAKE_INSTALL_PREFIX
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/umpire)

install(FILES
  ${PROJECT_BINARY_DIR}/umpire-config.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/umpire)

write_basic_package_version_file(
  ${PROJECT_BINARY_DIR}/umpire-config-version.cmake
  COMPATIBILITY SameMajorVersion)

install(FILES 
  "${PROJECT_BINARY_DIR}/umpire-config-version.cmake"
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/umpire)

install(
  EXPORT umpire-targets
  FILE umpire-targets.cmake
  NAMESPACE umpire::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/umpire)

if (UMPIRE_ENABLE_TESTS)
  add_subdirectory(tests)
endif ()

if (UMPIRE_ENABLE_DEVELOPER_BENCHMARKS)
  add_subdirectory(benchmarks)
  if ((NOT CMAKE_BUILD_TYPE) OR (NOT ${CMAKE_BUILD_TYPE} STREQUAL "Release"))
    message("-- Warning: CMAKE_BUILD_TYPE not set to Release, benchmark information will not be reliable for this build!")
  endif()
else()
  if (UMPIRE_ENABLE_BENCHMARKS)
    message("-- Warning: Benchmarks will not be built. If you want to build with benchmarks,\n"
          "     set UMPIRE_ENABLE_DEVELOPER_BENCHMARKS to On.")
  endif()
endif ()

if (UMPIRE_ENABLE_EXAMPLES)
  add_subdirectory(examples)
endif ()

if (UMPIRE_ENABLE_TOOLS)
  add_subdirectory(tools)
endif ()

if (UMPIRE_ENABLE_DOCS)
  add_subdirectory(docs)
endif ()
