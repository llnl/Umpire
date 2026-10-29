//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////

#include <iostream>

#include "umpire/Allocator.hpp"
#include "umpire/ResourceManager.hpp"
#include "umpire/Umpire.hpp"
#include "umpire/config.hpp"
#include "umpire/strategy/AlignedAllocator.hpp"
#include "umpire/strategy/SizeLimiter.hpp"

int main(int, char**)
{
  auto& rm = umpire::ResourceManager::getInstance();
  auto host = rm.getAllocator("HOST");

  bool constructor_failed{false};
  try {
    auto invalid = rm.makeAllocator<umpire::strategy::AlignedAllocator>("bad_alignment_allocator", host, 8);
    UMPIRE_USE_VAR(invalid);
  } catch (...) {
    constructor_failed = true;
  }

  if (!constructor_failed) {
    std::cerr << "Expected invalid aligned allocator construction to fail.\n";
    return 1;
  }

  auto limited = rm.makeAllocator<umpire::strategy::SizeLimiter>("size_limited_allocator", host, 1);

  bool allocation_failed{false};
  try {
    auto* ptr = limited.allocate(2);
    UMPIRE_USE_VAR(ptr);
  } catch (...) {
    allocation_failed = true;
  }

  if (!allocation_failed) {
    std::cerr << "Expected size-limited allocation to fail.\n";
    return 1;
  }

  return 0;
}
