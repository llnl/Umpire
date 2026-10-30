##############################################################################
# Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
# project contributors. See the COPYRIGHT file for details.
#
# SPDX-License-Identifier: (MIT)
##############################################################################
if (EXISTS ${SHROUD_EXECUTABLE})
  execute_process(COMMAND ${SHROUD_EXECUTABLE}
    --cmake ${CMAKE_CURRENT_BINARY_DIR}/SetupShroud.cmake
    ERROR_VARIABLE SHROUD_cmake_error
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  if (${SHROUD_cmake_error})
    message(FATAL_ERROR "Error from Shroud: ${SHROUD_cmake_error}")
  endif ()
  include(${CMAKE_CURRENT_BINARY_DIR}/SetupShroud.cmake)
endif ()

if (UMPIRE_ENABLE_UMAP)
  find_library( UMAP_LIBRARY
    libumap.so
    PATHS ${UMAP_ROOT}/lib
  )
  if (NOT UMAP_LIBRARY)
    message(FATAL_ERROR "Could not find UMAP library, check UMAP installation at UMAP_ROOT")
  endif()
  find_path( UMAP_INCLUDE_DIR
    NAMES "umap/umap.h"
    PATHS ${UMAP_ROOT}/include
  )
  if (NOT UMAP_INCLUDE_DIR)
    message(FATAL_ERROR "Headers missing, check UMAP installation at UMAP_ROOT")
  endif ()
  blt_import_library(NAME umap
    INCLUDES ${UMAP_INCLUDE_DIR}
    LIBRARIES ${UMAP_LIBRARY}
    DEPENDS_ON -lpthread -lrt)
endif ()

if (NOT "${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
  blt_register_library( NAME backtrace_symbols
    LIBRARIES ${CMAKE_DL_LIBS}
    )
endif ()

if (UMPIRE_ENABLE_SQLITE_EXPERIMENTAL)
  find_package(SQLite3 REQUIRED)
endif()

blt_install_tpl_setups(DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/umpire)