##############################################################################
# Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
# project contributors. See the COPYRIGHT file for details.
#
# SPDX-License-Identifier: (MIT)
##############################################################################
set (cookbook_depends umpire)

if (UMPIRE_ENABLE_OPENMP_TARGET)
  set (cookbook_depends
    ${cookbook_depends}
    openmp)
endif()

blt_add_executable(
  NAME recipe_no_introspection
  SOURCES recipe_no_introspection.cpp
  DEPENDS_ON ${cookbook_depends})
list(APPEND umpire_cookbooks recipe_no_introspection)

if (UMPIRE_ENABLE_NUMA)
  blt_add_executable(
    NAME recipe_move_between_numa
    SOURCES recipe_move_between_numa.cpp
    DEPENDS_ON ${cookbook_depends})
  blt_add_target_compile_flags(
    TO recipe_move_between_numa
    FLAGS ${UMPIRE_DISABLE_DEPRECATED_WARNINGS_FLAG})
  list(APPEND umpire_cookbooks recipe_move_between_numa)
endif ()

if (UMPIRE_ENABLE_IPC_SHARED_MEMORY)
  if (UMPIRE_ENABLE_MPI)
    blt_add_executable(
      NAME recipe_shared_memory
      SOURCES recipe_shared_memory.cpp
      DEPENDS_ON ${cookbook_depends})
    list(APPEND umpire_cookbooks recipe_shared_memory)

    if (UMPIRE_ENABLE_CUDA)
        blt_add_executable(
          NAME recipe_device_ipc
          SOURCES recipe_device_ipc.cpp
          DEPENDS_ON ${cookbook_depends} cuda_runtime)
        list(APPEND umpire_cookbooks recipe_device_ipc)
    elseif (UMPIRE_ENABLE_HIP)
        blt_add_executable(
          NAME recipe_device_ipc
          SOURCES recipe_device_ipc.cpp
          DEPENDS_ON ${cookbook_depends} blt::hip_runtime)
        list(APPEND umpire_cookbooks recipe_device_ipc)
    endif()
  endif()

  blt_add_executable(
    NAME recipe_naming_shim
    SOURCES recipe_naming_shim.cpp
    DEPENDS_ON ${cookbook_depends})
  list(APPEND umpire_cookbooks recipe_naming_shim)
endif()

if (UMPIRE_ENABLE_CUDA)
  blt_add_executable(
    NAME recipe_shrink
    SOURCES recipe_shrink.cpp
    DEPENDS_ON ${cookbook_depends})
  list(APPEND umpire_cookbooks recipe_shrink)

  blt_add_executable(
    NAME recipe_advice_device_id
    SOURCES recipe_advice_device_id.cpp
    DEPENDS_ON ${cookbook_depends})
  list(APPEND umpire_cookbooks recipe_advice_device_id)

  blt_add_executable(
    NAME recipe_pool_advice
    SOURCES recipe_pool_advice.cpp
    DEPENDS_ON ${cookbook_depends})
  list(APPEND umpire_cookbooks recipe_pool_advice)

  blt_add_executable(
    NAME recipe_move_to_managed
    SOURCES recipe_move_to_managed.cpp
    DEPENDS_ON ${cookbook_depends})
  blt_add_target_compile_flags(
    TO recipe_move_to_managed
    FLAGS ${UMPIRE_DISABLE_DEPRECATED_WARNINGS_FLAG})
  list(APPEND umpire_cookbooks recipe_move_to_managed)

  blt_add_executable(
    NAME recipe_use_advice_ops
    SOURCES recipe_use_advice_ops.cpp
    DEPENDS_ON ${cookbook_depends}
    OUTPUT_DIR ${cookbook_output_dir})
  list(APPEND umpire_cookbooks recipe_use_advice_ops)

  blt_add_executable(
    NAME recipe_memprefetch_pool
    SOURCES recipe_memprefetch_pool.cpp
    DEPENDS_ON ${cookbook_depends}
    OUTPUT_DIR ${cookbook_output_dir})
  list(APPEND umpire_cookbooks recipe_memprefetch_pool)

  if (UMPIRE_ENABLE_FORTRAN)
    blt_add_executable(
      NAME recipe_pinned_pool
      SOURCES recipe_pinned_pool.F
      DEPENDS_ON ${cookbook_depends})
    list(APPEND umpire_cookbooks recipe_pinned_pool)

    if (Fortran_COMPILER_FAMILY_IS_CLANG)
      set_source_files_properties(
        recipe_pinned_pool.F
        PROPERTIES
        COMPILE_FLAGS -Mfree)
    else()
      set_source_files_properties(
        recipe_pinned_pool.F
        PROPERTIES
        Fortran_FORMAT FREE)
    endif()

    blt_add_executable(
      NAME recipe_prefetch_pool
      SOURCES recipe_prefetch_pool.F
      DEPENDS_ON ${cookbook_depends}
      OUTPUT_DIR ${cookbook_output_dir})
    list(APPEND umpire_cookbooks recipe_prefetch_pool)

    if (Fortran_COMPILER_FAMILY_IS_CLANG)
      set_source_files_properties(
        recipe_prefetch_pool.F
        PROPERTIES
        COMPILE_FLAGS -Mfree)
    else()
      set_source_files_properties(
        recipe_prefetch_pool.F
        PROPERTIES
        Fortran_FORMAT FREE)
    endif()
  endif()
endif ()

if (UMPIRE_ENABLE_HIP)
  if (UMPIRE_ENABLE_HIP_COHERENCE_GRANULARITY)
    blt_add_executable(
      NAME recipe_control_granularity
      SOURCES recipe_control_granularity.cpp
      DEPENDS_ON ${cookbook_depends})
    list(APPEND umpire_cookbooks recipe_control_granularity)
  endif ()

  blt_add_executable(
    NAME recipe_shrink
    SOURCES recipe_shrink.cpp
    DEPENDS_ON ${cookbook_depends})
  list(APPEND umpire_cookbooks recipe_shrink)

  blt_add_executable(
    NAME recipe_move_to_managed
    SOURCES recipe_move_to_managed.cpp
    DEPENDS_ON ${cookbook_depends})
  list(APPEND umpire_cookbooks recipe_move_to_managed)

  if (UMPIRE_ENABLE_FORTRAN)
    blt_add_executable(
      NAME recipe_pinned_pool
      SOURCES recipe_pinned_pool.F
      DEPENDS_ON ${cookbook_depends})
    list(APPEND umpire_cookbooks recipe_pinned_pool)

    if (Fortran_COMPILER_FAMILY_IS_CLANG)
      set_source_files_properties(
        recipe_pinned_pool.F
        PROPERTIES
        COMPILE_FLAGS -Mfree)
    else ()
      set_source_files_properties(
        recipe_pinned_pool.F
        PROPERTIES
        Fortran_FORMAT FREE)
    endif ()
  endif ()
endif ()

blt_add_executable(
  NAME recipe_quick_pool_heuristic
  SOURCES recipe_quick_pool_heuristic.cpp
  DEPENDS_ON ${cookbook_depends})
list(APPEND umpire_cookbooks recipe_quick_pool_heuristic)

blt_add_executable(
  NAME recipe_coalesce_pool
  SOURCES recipe_coalesce_pool.cpp
  DEPENDS_ON ${cookbook_depends})
list(APPEND umpire_cookbooks recipe_coalesce_pool)

blt_add_executable(
  NAME recipe_get_largest_available_block_in_pool
  SOURCES recipe_get_largest_available_block_in_pool.cpp
  DEPENDS_ON ${cookbook_depends})
list(APPEND umpire_cookbooks recipe_get_largest_available_block_in_pool)

blt_add_executable(
  NAME recipe_plot_allocations
  SOURCES recipe_plot_allocations.cpp
  DEPENDS_ON ${cookbook_depends})
list(APPEND umpire_cookbooks recipe_plot_allocations)

blt_add_executable(
  NAME recipe_thread_safe
  SOURCES recipe_thread_safe.cpp
  DEPENDS_ON ${cookbook_depends})
list(APPEND umpire_cookbooks recipe_thread_safe)

blt_add_executable(
  NAME recipe_mixed_pool
  SOURCES recipe_mixed_pool.cpp
  DEPENDS_ON ${cookbook_depends})
list(APPEND umpire_cookbooks recipe_mixed_pool)

if (UMPIRE_ENABLE_FORTRAN)
  blt_add_executable(
    NAME recipe_thread_safe_F
    SOURCES recipe_thread_safe.F
    DEPENDS_ON ${cookbook_depends})

  if (Fortran_COMPILER_FAMILY_IS_CLANG)
    set_source_files_properties(
      recipe_thread_safe.F
      PROPERTIES
      COMPILE_FLAGS -Mfree)
  else()
    set_source_files_properties(
      recipe_thread_safe.F
      PROPERTIES
      Fortran_FORMAT FREE)
  endif()
endif ()

blt_add_executable(
  NAME recipe_strategy_name
  SOURCES recipe_strategy_name.cpp
  DEPENDS_ON ${cookbook_depends})
list(APPEND umpire_cookbooks recipe_strategy_name)

blt_add_executable(
  NAME recipe_named_allocation
  SOURCES recipe_named_allocation.cpp
  DEPENDS_ON ${cookbook_depends})
list(APPEND umpire_cookbooks recipe_named_allocation)

install(TARGETS ${umpire_cookbooks} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
umpire_add_code_checks(PREFIX cookbook)
