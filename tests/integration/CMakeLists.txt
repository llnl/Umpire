##############################################################################
# Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
# project contributors. See the COPYRIGHT file for details.
#
# SPDX-License-Identifier: (MIT)
##############################################################################
set (integration_tests_depends
  umpire gtest)
set (accessibility_test_depends
  umpire gtest)
set (device_allocator_test_depends
  umpire_device umpire gtest)

if (UMPIRE_ENABLE_OPENMP OR UMPIRE_ENABLE_OPENMP_TARGET)
  set (integration_tests_depends
    ${integration_tests_depends}
    openmp)
endif ()

if (UMPIRE_ENABLE_CUDA)
  set (integration_tests_depends
    ${integration_tests_depends}
    cuda 
    cuda_runtime)
  set (accessibility_test_depends
    ${accessibility_test_depends}
    cuda)
  set (device_allocator_test_depends
    ${device_allocator_test_depends}
    cuda)
endif ()

if (UMPIRE_ENABLE_HIP)
  set (integration_tests_depends
    ${integration_tests_depends}
    blt::hip 
    blt::hip_runtime)
  set (accessibility_test_depends
    ${accessibility_test_depends}
    blt::hip)
  set (device_allocator_test_depends
    ${device_allocator_test_depends}
    blt::hip)
endif ()

if (UMPIRE_ENABLE_OPENMP OR UMPIRE_ENABLE_OPENMP_TARGET)
  set (accessibility_test_depends
    ${accessibility_test_depends}
    openmp)
endif ()

blt_add_executable(
  NAME allocator_integration_tests
  SOURCES allocator_integration_tests.cpp
  DEPENDS_ON ${integration_tests_depends})

target_include_directories(
  allocator_integration_tests
  PRIVATE
  ${PROJECT_BINARY_DIR}/include)

blt_add_test(
  NAME allocator_integration_tests
  COMMAND allocator_integration_tests)

blt_add_executable(
  NAME typed_allocator_integration_tests
  SOURCES typed_allocator_integration_tests.cpp
  DEPENDS_ON ${integration_tests_depends})

target_include_directories(
  typed_allocator_integration_tests
  PRIVATE
  ${PROJECT_BINARY_DIR}/include)

blt_add_test(
  NAME typed_allocator_integration_tests
  COMMAND typed_allocator_integration_tests)

blt_add_executable(
  NAME allocator_accessibility_tests
  SOURCES allocator_accessibility.cpp
  DEPENDS_ON ${accessibility_test_depends})

target_include_directories(
  allocator_accessibility_tests
  PRIVATE
  ${PROJECT_BINARY_DIR}/include)

blt_add_test(
  NAME allocator_accessibility_tests
  COMMAND allocator_accessibility_tests)

if (UMPIRE_ENABLE_DEVICE_ALLOCATOR)
  blt_add_executable(
    NAME device_allocator_tests
    SOURCES device_allocator.cpp
    DEPENDS_ON ${device_allocator_test_depends})

  target_include_directories(
    device_allocator_tests
    PRIVATE
    ${PROJECT_BINARY_DIR}/include)

  if (UMPIRE_ENABLE_CUDA)
    set_source_files_properties(
      device-allocator.cpp
      PROPERTIES CUDA_SEPARABLE_COMPILATION On)

    set_target_properties(
      device_allocator_tests
      PROPERTIES CUDA_SEPARABLE_COMPILATION On)
  endif()
  if (UMPIRE_ENABLE_HIP)
    target_compile_options(device_allocator_tests PUBLIC -fgpu-rdc)
    blt_add_target_link_flags( TO device_allocator_tests FLAGS -fgpu-rdc)
  endif()

  blt_add_test(
    NAME device_allocator_tests
    COMMAND device_allocator_tests)
endif()

blt_add_executable(
  NAME memory_resource_traits_tests
  SOURCES memory_resource_traits_tests.cpp
  DEPENDS_ON ${integration_tests_depends})

target_include_directories(
  memory_resource_traits_tests
  PRIVATE
  ${PROJECT_BINARY_DIR}/include)

blt_add_test(
  NAME memory_resource_traits_tests
  COMMAND memory_resource_traits_tests)

blt_add_executable(
  NAME strategy_tests
  SOURCES strategy_tests.cpp
  DEPENDS_ON ${integration_tests_depends})

target_include_directories(
  strategy_tests
  PRIVATE
  ${PROJECT_BINARY_DIR}/include)

blt_add_test(
  NAME strategy_tests
  COMMAND strategy_tests)

blt_add_executable(
  NAME resource_aware_pool_tests
  SOURCES resource_aware_pool_tests.cpp
  DEPENDS_ON ${integration_tests_depends})

blt_add_test(
  NAME resource_aware_pool_tests
  COMMAND resource_aware_pool_tests)

blt_add_executable(
  NAME operation_tests
  SOURCES operation_tests.cpp
  DEPENDS_ON ${integration_tests_depends})

set_source_files_properties(operation_tests.cpp PROPERTIES COMPILE_FLAGS ${UMPIRE_DISABLE_DEPRECATED_WARNINGS_FLAG})
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
  set_source_files_properties(operation_tests.cpp PROPERTIES COMPILE_FLAGS /bigobj)
endif()

target_include_directories(
  operation_tests
  PRIVATE
  ${PROJECT_BINARY_DIR}/include)

blt_add_test(
  NAME operation_tests
  COMMAND operation_tests)

blt_add_executable(
  NAME reallocate_tests
  SOURCES reallocate_tests.cpp
  DEPENDS_ON ${integration_tests_depends})

target_include_directories(
  reallocate_tests
  PRIVATE
  ${PROJECT_BINARY_DIR}/include)

blt_add_test(
  NAME reallocate_tests
  COMMAND reallocate_tests)

blt_add_executable(
  NAME free_functions_integration_tests
  SOURCES free_functions_integration_tests.cpp
  DEPENDS_ON ${integration_tests_depends})

target_include_directories(
  free_functions_integration_tests
  PRIVATE
  ${PROJECT_BINARY_DIR}/include)

blt_add_test(
  NAME free_functions_integration_tests
  COMMAND free_functions_integration_tests)

blt_add_executable(
  NAME primary_pool_tests
  SOURCES primary_pool_tests.cpp
  DEPENDS_ON ${integration_tests_depends})

blt_add_test(
  NAME primary_pool_tests
  COMMAND primary_pool_tests)

if (UMPIRE_ENABLE_PERFORMANCE_TESTS)
  blt_add_executable(
    NAME primary_pool_performance_tests
    SOURCES primary_pool_performance_tests.cpp
    DEPENDS_ON ${integration_tests_depends})

  blt_add_test(
    NAME primary_pool_performance_tests
    COMMAND primary_pool_performance_tests)
endif()

blt_add_executable(
  NAME static_initialization_test
  SOURCES static_initialization_test.cpp
  DEPENDS_ON ${integration_tests_depends})

blt_add_test(
  NAME static_initialization_test
  COMMAND static_initialization_test)

if (UMPIRE_ENABLE_INTROSPECTION_HEADER)
  blt_add_executable(
    NAME introspection_header_tests
    SOURCES introspection_header_tests.cpp
    DEPENDS_ON ${integration_tests_depends})

  target_include_directories(
    introspection_header_tests
    PRIVATE
    ${PROJECT_BINARY_DIR}/include)

  blt_add_test(
    NAME introspection_header_tests
    COMMAND introspection_header_tests)
else ()
  blt_add_executable(
    NAME introspection_tests
    SOURCES introspection_tests.cpp
    DEPENDS_ON ${integration_tests_depends})

  blt_add_test(
    NAME introspection_tests
    COMMAND introspection_tests)
endif ()

blt_add_executable(
  NAME destroy_allocator_tests
  SOURCES destroy_allocator_tests.cpp
  DEPENDS_ON ${integration_tests_depends})

target_include_directories(
  destroy_allocator_tests
  PRIVATE
  ${PROJECT_BINARY_DIR}/include)

if (UMPIRE_ENABLE_MPI AND (UMPIRE_ENABLE_IPC_SHARED_MEMORY OR UMPIRE_ENABLE_MPI3_SHARED_MEMORY))
  umpire_add_test_with_mpi(
    NAME destroy_allocator_tests
    COMMAND destroy_allocator_tests
    NUM_MPI_TASKS 1)
else ()
  blt_add_test(
    NAME destroy_allocator_tests
    COMMAND destroy_allocator_tests)
endif ()

blt_add_executable(
  NAME destroy_allocator_strict_mode_tests
  SOURCES destroy_allocator_strict_mode_tests.cpp
  DEPENDS_ON ${integration_tests_depends})

target_include_directories(
  destroy_allocator_strict_mode_tests
  PRIVATE
  ${PROJECT_BINARY_DIR}/include)

blt_add_test(
  NAME destroy_allocator_strict_mode_tests
  COMMAND destroy_allocator_strict_mode_tests)

set_property(TEST destroy_allocator_strict_mode_tests
  PROPERTY ENVIRONMENT "UMPIRE_STRICT_DESTRUCTION=1")

if (UMPIRE_ENABLE_MPI AND (UMPIRE_ENABLE_IPC_SHARED_MEMORY OR UMPIRE_ENABLE_MPI3_SHARED_MEMORY))
  blt_add_executable(
    NAME get_communicator_tests
    SOURCES get_communicator_tests.cpp
    DEPENDS_ON ${integration_tests_depends})

  umpire_add_test_with_mpi(
    NAME get_communicator_tests
    COMMAND get_communicator_tests
    NUM_MPI_TASKS 2)
endif ()

if (UMPIRE_ENABLE_TOOLS)
  if (NOT UMPIRE_ENABLE_HIP)
    add_subdirectory(replay)
  else ()
    message(STATUS "Disabling replay tests for HIP build.")
  endif ()
endif ()

if (UMPIRE_ENABLE_C OR UMPIRE_ENABLE_FORTRAN)
  add_subdirectory(interface)
endif()

add_subdirectory(io)

blt_add_target_compile_flags(
  TO strategy_tests
  FLAGS ${UMPIRE_DISABLE_DEPRECATED_WARNINGS_FLAG})
blt_add_target_compile_flags(
  TO operation_tests
  FLAGS ${UMPIRE_DISABLE_DEPRECATED_WARNINGS_FLAG})
blt_add_target_compile_flags(
  TO primary_pool_tests
  FLAGS ${UMPIRE_DISABLE_DEPRECATED_WARNINGS_FLAG})
if (UMPIRE_ENABLE_PERFORMANCE_TESTS)
  blt_add_target_compile_flags(
    TO primary_pool_performance_tests 
    FLAGS ${UMPIRE_DISABLE_DEPRECATED_WARNINGS_FLAG})
endif()
