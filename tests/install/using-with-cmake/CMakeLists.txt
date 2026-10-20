##############################################################################
# Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
# project contributors. See the COPYRIGHT file for details.
#
# SPDX-License-Identifier: (MIT)
##############################################################################

cmake_minimum_required(VERSION 3.23)

project(using_with_cmake)

if(NOT DEFINED umpire_DIR)
    message(FATAL_ERROR "Missing required 'umpire_DIR' variable pointing to an installed Umpire")
endif()

find_package(umpire REQUIRED
             NO_DEFAULT_PATH 
             PATHS ${umpire_DIR})

# Check if fmt::fmt was imported from above find_package
if(NOT TARGET fmt::fmt)
    message(FATAL_ERROR "fmt targets were not imported from installed config files.")
endif()

add_executable(using-with-cmake using-with-cmake.cpp)
target_link_libraries(using-with-cmake umpire::umpire fmt::fmt)
