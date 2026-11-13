##############################################################################
# Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
# project contributors. See the COPYRIGHT file for details.
#
# SPDX-License-Identifier: (MIT)
##############################################################################

# With intel oneapi (as of version 2022.[1|2]) we see errors building gtest
# due to the default flag "-ffp-model=fast" (relates to comparing to NaN).
if(C_COMPILER_FAMILY_IS_INTELLLVM)
  target_compile_options(gtest PRIVATE -ffp-model=precise)
  target_compile_options(gtest_main PRIVATE -ffp-model=precise)
endif()

add_subdirectory(tpl)
add_subdirectory(unit)
add_subdirectory(integration)
add_subdirectory(tools)
add_subdirectory(debug)
add_subdirectory(op)
if (UMPIRE_ENABLE_DEVELOPER_DEFAULTS)
    add_subdirectory(applications)
endif()
add_subdirectory(install)

umpire_add_code_checks(PREFIX tests RECURSIVE TRUE)
