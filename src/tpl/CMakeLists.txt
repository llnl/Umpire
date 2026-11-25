##############################################################################
# Copyright (c) 2018, Lawrence Livermore National Security, LLC.
# Produced at the Lawrence Livermore National Laboratory
#
# Created by David Beckingsale, david@llnl.gov
# LLNL-CODE-747640
#
# All rights reserved.
#
# This file is part of Umpire.
#
# For details, see https://github.com/LLNL/Umpire
# Please also see the LICENSE file for MIT license.
##############################################################################

set(OLD_ENABLE_TESTS ${UMPIRE_ENABLE_TESTS})
set(OLD_ENABLE_FORTRAN ${UMPIRE_ENABLE_FORTRAN})
set(UMPIRE_ENABLE_TESTS Off)
set(UMPIRE_ENABLE_FORTRAN Off)

#
# Json
set (json_headers
  umpire/json/json.hpp )

blt_add_library(
  NAME umpire_tpl_json
  HEADERS ${json_headers})

if (C_COMPILER_FAMILY_IS_INTEL)
  target_compile_options(
    umpire_tpl_json
    INTERFACE
    $<$<COMPILE_LANGUAGE:CXX>:-wd186>
    $<$<COMPILE_LANGUAGE:C>:-wd186>
  )
endif ()

if (C_COMPILER_FAMILY_IS_PGI)
  target_compile_options(
    umpire_tpl_json
    INTERFACE
    $<$<COMPILE_LANGUAGE:CXX>:--diag_suppress 186>
  )
endif ()

target_link_libraries(umpire_tpl_json INTERFACE)

target_include_directories(
  umpire_tpl_json
  INTERFACE
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src/tpl>
  $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>)

blt_convert_to_system_includes(TARGET umpire_tpl_json)

target_include_directories(
  umpire_tpl_json
  INTERFACE
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

install(FILES
  ${json_headers}
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/umpire/json)

install(TARGETS
  umpire_tpl_json
  EXPORT umpire-targets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})


#
# CLI11 Option Parsing Headers
set (CLI11_headers
  umpire/CLI11/CLI11.hpp )

blt_add_library(
  NAME umpire_tpl_CLI11
  HEADERS ${CLI11_headers})

target_link_libraries(umpire_tpl_CLI11 INTERFACE)

target_include_directories(
  umpire_tpl_CLI11
  INTERFACE
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src/tpl>
  $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>)

blt_convert_to_system_includes(TARGET umpire_tpl_CLI11)

target_include_directories(
  umpire_tpl_CLI11
  INTERFACE
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

install(FILES
  ${CLI11_headers}
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/umpire/CLI11)

install(TARGETS
  umpire_tpl_CLI11
  EXPORT umpire-targets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

add_subdirectory(umpire/judy)

if (NOT TARGET camp)
  if (DEFINED camp_DIR)
    find_package(camp REQUIRED
      NO_DEFAULT_PATH
      PATHS ${camp_DIR}
      ${camp_DIR}/lib/cmake/camp
    )
    set_target_properties(camp PROPERTIES IMPORTED_GLOBAL TRUE)
    set(UMPIRE_CONFIG_camp_DIR ${camp_DIR} CACHE PATH "")
  else ()
    # In this case, camp will be installed inside of Umpire install prefix
    set(UMPIRE_CONFIG_camp_DIR ${CMAKE_INSTALL_PREFIX} CACHE PATH "")
    add_subdirectory(umpire/camp)
  endif()

  if(UMPIRE_ENABLE_CUDA)
    blt_add_target_definitions(
      TO camp
      SCOPE INTERFACE
      TARGET_DEFINITIONS CAMP_HAVE_CUDA)
  endif()

  if(ENABLE_HIP)
    blt_add_target_definitions(
      TO camp
      SCOPE INTERFACE
      TARGET_DEFINITIONS CAMP_HAVE_HIP)
  endif()
endif ()

if (NOT TARGET ${UMPIRE_FMT_TARGET})
  if (DEFINED fmt_DIR)
    find_package(fmt REQUIRED
      NO_DEFAULT_PATH
      PATHS
        ${fmt_DIR}
        ${fmt_DIR}/lib64/cmake/fmt)

    set_target_properties(${UMPIRE_FMT_TARGET} PROPERTIES IMPORTED_GLOBAL TRUE)
    blt_convert_to_system_includes(TARGET ${UMPIRE_FMT_TARGET})
    set(UMPIRE_CONFIG_fmt_DIR ${fmt_DIR} CACHE PATH "")
  else ()
    if (NOT EXISTS ${PROJECT_SOURCE_DIR}/src/tpl/umpire/fmt/CMakeLists.txt)
      message(FATAL_ERROR "fmt submodule not initialized. Run 'git submodule update --init --recursive' in the git repository or set fmt_DIR to use an external build of fmt.")
    else ()
      # In this case, fmt will be installed inside of Umpire install prefix
      set(UMPIRE_CONFIG_fmt_DIR ${CMAKE_INSTALL_PREFIX} CACHE PATH "")
      set(FMT_INSTALL ON)
      set(FMT_SYSTEM_HEADERS ON)
      add_subdirectory(umpire/fmt)
    endif ()
  endif ()
endif ()

##############################################################################
# spdlog (only required when logging is enabled)
##############################################################################
if (UMPIRE_ENABLE_LOGGING AND NOT TARGET spdlog::spdlog)
  # Try to find external spdlog installation
  if (DEFINED spdlog_DIR)
    find_package(spdlog REQUIRED
      NO_DEFAULT_PATH
      PATHS
        ${spdlog_DIR}
        ${spdlog_DIR}/lib/cmake/spdlog
        ${spdlog_DIR}/lib64/cmake/spdlog)

    set_target_properties(spdlog::spdlog PROPERTIES IMPORTED_GLOBAL TRUE)
    blt_convert_to_system_includes(TARGET spdlog::spdlog)
    set(UMPIRE_CONFIG_spdlog_DIR ${spdlog_DIR} CACHE PATH "")
  else ()
    if (NOT EXISTS ${PROJECT_SOURCE_DIR}/src/tpl/umpire/spdlog/CMakeLists.txt)
      message(FATAL_ERROR "spdlog submodule not initialized. Run 'git submodule update --init --recursive' in the git repository or set spdlog_DIR to use an external build of spdlog.")
    else ()
      # Configure spdlog to use external fmt (if available)
      if (TARGET ${UMPIRE_FMT_TARGET})
        message(STATUS "Configuring spdlog to use external fmt library: ${UMPIRE_FMT_TARGET}")

        # Help spdlog find the fmt package by setting fmt_DIR
        # This works whether fmt is external or from our submodule
        if (DEFINED fmt_DIR)
          # External fmt case - use the user-provided path
          message(STATUS "Using external fmt from: ${fmt_DIR}")
          set(fmt_DIR ${fmt_DIR} CACHE PATH "Path to fmt")

          # Check if the external fmt is header-only
          # Note: SPDLOG_FMT_EXTERNAL and SPDLOG_FMT_EXTERNAL_HO are mutually exclusive
          get_target_property(fmt_type ${UMPIRE_FMT_TARGET} TYPE)
          if (fmt_type STREQUAL "INTERFACE_LIBRARY")
            message(STATUS "External fmt is header-only, setting SPDLOG_FMT_EXTERNAL_HO=ON")
            set(SPDLOG_FMT_EXTERNAL_HO ON CACHE BOOL "Use external fmt header-only library")
            set(SPDLOG_FMT_EXTERNAL OFF CACHE BOOL "Use external fmt library")
          else()
            message(STATUS "External fmt is compiled library, setting SPDLOG_FMT_EXTERNAL=ON")
            set(SPDLOG_FMT_EXTERNAL ON CACHE BOOL "Use external fmt library")
            set(SPDLOG_FMT_EXTERNAL_HO OFF CACHE BOOL "Use external fmt header-only library")
            if (NOT DEFINED SPDLOG_FMT_EXTERNAL_HO)
              message(STATUS "If you encounter linking issues, you may need to set SPDLOG_FMT_EXTERNAL_HO=ON")
            endif()
          endif()
        else ()
          # Submodule fmt case - point to build directory where fmt exports its config.
          # Umpire uses the fmt::fmt-header-only target from the submodule, so use SPDLOG_FMT_EXTERNAL_HO
          message(STATUS "Using Umpire's bundled fmt submodule (header-only mode)")
          set(SPDLOG_FMT_EXTERNAL_HO ON CACHE BOOL "Use external fmt header-only library")
          set(SPDLOG_FMT_EXTERNAL OFF CACHE BOOL "Use external fmt library")
          set(fmt_DIR ${CMAKE_BINARY_DIR}/src/tpl/umpire/fmt CACHE PATH "Path to fmt")
        endif ()
      else ()
        # No fmt available, let spdlog use its bundled fmt
        message(STATUS "No external fmt found, spdlog will use its bundled fmt")
        set(SPDLOG_FMT_EXTERNAL OFF CACHE BOOL "Use external fmt library")
        set(SPDLOG_FMT_EXTERNAL_HO OFF CACHE BOOL "Use external fmt header-only library")
      endif ()

      # Disable unnecessary spdlog features
      set(SPDLOG_INSTALL ON CACHE BOOL "Generate install target")
      set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "Build shared library")

      # In this case, spdlog will be installed inside of Umpire install prefix
      set(UMPIRE_CONFIG_spdlog_DIR ${CMAKE_INSTALL_PREFIX} CACHE PATH "")

      add_subdirectory(umpire/spdlog)
    endif ()
  endif ()
endif ()

set(UMPIRE_ENABLE_TESTS ${OLD_ENABLE_TESTS})
set(UMPIRE_ENABLE_FORTRAN ${OLD_ENABLE_FORTRAN})
