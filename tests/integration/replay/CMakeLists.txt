##############################################################################
# Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
# project contributors. See the COPYRIGHT file for details.
#
# SPDX-License-Identifier: (MIT)
##############################################################################
find_program (BASH_PROGRAM bash)

set (replay_integration_tests_depends
  umpire)

if (UMPIRE_ENABLE_OPENMP_TARGET)
  set (replay_integration_tests_depends
      ${replay_integration_tests_depends}
      openmp)
endif()

blt_add_executable(
  NAME replay_tests
  SOURCES replay_tests.cpp
  DEPENDS_ON ${replay_integration_tests_depends})

blt_add_executable(
  NAME replay_failure_tests
  SOURCES replay_failure_tests.cpp
  DEPENDS_ON ${replay_integration_tests_depends})

if (BASH_PROGRAM)
  if (NOT C_COMPILER_FAMILY_IS_PGI)
    add_test(
      NAME replay_tests
      COMMAND ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/replay_tests.bash ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

    add_test(
      NAME replay_failure_tests
      COMMAND ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/replay_failure_tests.bash ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
  endif()
else ()
  message(STATUS "Bash not configured, replay tests disabled.")
endif ()
