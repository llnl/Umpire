##############################################################################
# Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
# project contributors. See the COPYRIGHT file for details.
#
# SPDX-License-Identifier: (MIT)
##############################################################################
set (tutorial_depends umpire)

if (UMPIRE_ENABLE_OPENMP_TARGET)
  set (tutorial_depends
    ${tutorial_depends}
    openmp)
endif ()

blt_add_executable(
  NAME tut_allocator
  SOURCES tut_allocator.cpp
  DEPENDS_ON ${tutorial_depends})
list(APPEND umpire_tutorials tut_allocator)

blt_add_executable(
  NAME tut_resources
  SOURCES tut_resources.cpp
  DEPENDS_ON ${tutorial_depends})
list(APPEND umpire_tutorials tut_resources)

blt_add_executable(
  NAME tut_quick_pool_1
  SOURCES tut_quick_pool_1.cpp
  DEPENDS_ON ${tutorial_depends})
list(APPEND umpire_tutorials tut_quick_pool_1)

blt_add_executable(
  NAME tut_quick_pool_2
  SOURCES tut_quick_pool_2.cpp
  DEPENDS_ON ${tutorial_depends})
list(APPEND umpire_tutorials tut_quick_pool_2)

blt_add_executable(
  NAME tut_copy
  SOURCES tut_copy.cpp
  DEPENDS_ON ${tutorial_depends})
list(APPEND umpire_tutorials tut_copy)

blt_add_executable(
  NAME tut_move
  SOURCES tut_move.cpp
  DEPENDS_ON ${tutorial_depends})
list(APPEND umpire_tutorials tut_move)

blt_add_executable(
  NAME tut_memset
  SOURCES tut_memset.cpp
  DEPENDS_ON ${tutorial_depends})
list(APPEND umpire_tutorials tut_memset)

blt_add_executable(
  NAME tut_reallocate
  SOURCES tut_reallocate.cpp
  DEPENDS_ON ${tutorial_depends})
list(APPEND umpire_tutorials tut_reallocate)

blt_add_executable(
  NAME tut_typed_allocator
  SOURCES tut_typed_allocator.cpp
  DEPENDS_ON ${tutorial_depends})
list(APPEND umpire_tutorials tut_typed_allocator)

blt_add_executable(
  NAME tut_introspection
  SOURCES tut_introspection.cpp
  DEPENDS_ON ${tutorial_depends})
list(APPEND umpire_tutorials tut_introspection)

blt_add_executable(
  NAME tut_replay
  SOURCES tut_replay.cpp
  DEPENDS_ON ${tutorial_depends})
list(APPEND umpire_tutorials tut_replay)

if (UMPIRE_ENABLE_C)
  add_subdirectory(c)
endif ()

if (UMPIRE_ENABLE_FORTRAN)
  add_subdirectory(fortran)
endif ()

install(TARGETS ${umpire_tutorials} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

umpire_add_code_checks(PREFIX tutorial)
