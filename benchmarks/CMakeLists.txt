##############################################################################
# Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
# project contributors. See the COPYRIGHT file for details.
#
# SPDX-License-Identifier: (MIT)
##############################################################################
set (stress_test_depends umpire)

if (UMPIRE_ENABLE_OPENMP_TARGET)
  set (stress_test_depends
      ${stress_test_depends}
      openmp)
endif()

blt_add_executable(
  NAME allocator_stress_test
  SOURCES allocator_stress_test.cpp
  DEPENDS_ON ${stress_test_depends})

if (UMPIRE_ENABLE_CUDA)
  blt_add_executable(
    NAME resource_aware_pool_stress_test
    SOURCES resource_aware_pool_stress_test.cpp
    DEPENDS_ON umpire cuda)

  if (UMPIRE_ENABLE_DEVICE_ALLOCATOR)
  blt_add_executable(
    NAME device_allocator_stress_test
    SOURCES device_allocator_stress_test.cpp
    DEPENDS_ON umpire_device umpire cuda)

  set_source_files_properties(
    device_allocator_stress_test.cpp
    PROPERTIES CUDA_SEPARABLE_COMPILATION On)

  set_target_properties(
    device_allocator_stress_test
    PROPERTIES CUDA_SEPARABLE_COMPILATION On)
  endif()
elseif (UMPIRE_ENABLE_HIP)
  blt_add_executable(
    NAME resource_aware_pool_stress_test
    SOURCES resource_aware_pool_stress_test.cpp
    DEPENDS_ON umpire blt::hip blt::hip_runtime)

  if (UMPIRE_ENABLE_DEVICE_ALLOCATOR)
    blt_add_executable(
      NAME device_allocator_stress_test
      SOURCES device_allocator_stress_test.cpp
      DEPENDS_ON umpire_device umpire blt::hip blt::hip_runtime)

    set_target_properties(
      device_allocator_stress_test
      PROPERTIES LINK_FLAGS "-fgpu-rdc")
  endif()
endif()

blt_add_executable(
  NAME allocator_memory_cost_benchmark
  SOURCES allocator_memory_cost_benchmark.cpp
  DEPENDS_ON ${stress_test_depends})

blt_add_executable(
  NAME copy_stress_test
  SOURCES copy_stress_test.cpp
  DEPENDS_ON ${stress_test_depends})

blt_add_executable(
  NAME fixed_pool_stress_test
  SOURCES fixed_pool_stress_test.cpp
  DEPENDS_ON ${stress_test_depends})

blt_add_executable(
  NAME no-op_stress_test
  SOURCES no-op_stress_test.cpp
  DEPENDS_ON ${stress_test_depends})

blt_add_benchmark(
  NAME no-op_stress_test
  COMMAND no-op_stress_test)

blt_add_executable(
  NAME pool_stress_test
  SOURCES pool_stress_test.cpp
  DEPENDS_ON ${stress_test_depends})

blt_add_target_compile_flags(
  TO pool_stress_test
  FLAGS ${UMPIRE_DISABLE_DEPRECATED_WARNINGS_FLAG})

if (UMPIRE_ENABLE_BENCHMARKS)
  set (benchmark_depends gbenchmark umpire)

  if (UMPIRE_ENABLE_OPENMP_TARGET)
    set (benchmark_depends
      ${benchmark_depends}
      openmp)
  endif()

  blt_add_executable(
    NAME allocator_benchmarks
    SOURCES allocator_benchmarks.cpp
    DEPENDS_ON ${benchmark_depends})

  blt_add_benchmark(
    NAME allocator_benchmarks
    COMMAND allocator_benchmarks)

  blt_add_executable(
    NAME vendor_allocator_benchmarks
    SOURCES vendor_allocator_benchmarks.cpp
    DEPENDS_ON ${benchmark_depends})

  blt_add_benchmark(
    NAME vendor_allocator_benchmarks
    COMMAND vendor_allocator_benchmarks)

  blt_add_executable(
    NAME debuglog_benchmarks
    SOURCES debuglog_benchmarks.cpp
    DEPENDS_ON ${benchmark_depends})

  if (UMPIRE_ENABLE_OPENMP)
    set (benchmark_depends
      ${benchmark_depends}
      openmp)

    blt_add_executable(
      NAME file_resource_benchmarks
      SOURCES file_resource_benchmarks.cpp
      DEPENDS_ON ${benchmark_depends})
  endif()

  blt_add_executable(
    NAME copy_benchmarks
    SOURCES copy_benchmarks.cpp
    DEPENDS_ON ${benchmark_depends})

  blt_add_benchmark(
    NAME copy_benchmarks
    COMMAND copy_benchmarks)

  blt_add_executable(
    NAME inspector_benchmarks
    SOURCES inspector_benchmarks.cpp
    DEPENDS_ON ${benchmark_depends})

  blt_add_benchmark(
    NAME inspector_benchmarks
    COMMAND inspector_benchmarks)

  blt_add_executable(
    NAME introspection_header_benchmarks
    SOURCES introspection_header_benchmarks.cpp
    DEPENDS_ON ${benchmark_depends})

  blt_add_benchmark(
    NAME introspection_header_benchmarks
    COMMAND introspection_header_benchmarks)
endif()
