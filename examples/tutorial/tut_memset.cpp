//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#include "umpire/Allocator.hpp"
#include "umpire/ResourceManager.hpp"
#include "umpire/op.hpp"

int main(int, char**)
{
  constexpr std::size_t SIZE = 1024;

  auto& rm = umpire::ResourceManager::getInstance();

  const std::string destinations[] = {
    "HOST"
#if defined(UMPIRE_ENABLE_DEVICE)
    ,
    "DEVICE"
#endif
#if defined(UMPIRE_ENABLE_UM)
    ,
    "UM"
#endif
#if defined(UMPIRE_ENABLE_PINNED)
    ,
    "PINNED"
#endif
  };

  for (auto& destination : destinations) {
    auto allocator = rm.getAllocator(destination);
    double* data = static_cast<double*>(allocator.allocate(SIZE * sizeof(double)));

    std::cout << "Allocated " << (SIZE * sizeof(double)) << " bytes using the " << allocator.getName() << " allocator."
              << std::endl;

    // _sphinx_tag_tut_memset_start
    umpire::memset(data, 0, SIZE);
    // _sphinx_tag_tut_memset_end

    std::cout << "Set data from " << destination << " (" << data << ") to 0." << std::endl;

    allocator.deallocate(data);
  }

  return 0;
}
