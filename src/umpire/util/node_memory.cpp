//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////

#include "umpire/util/node_memory.hpp"

#include <fstream>
#include <sstream>
#include <string>

namespace umpire {
namespace util {

double get_node_available_memory(double default_MiB)
{
  double available_MiB = default_MiB;

#if defined(__linux__)
  constexpr double bytes_per_MiB = 1024.0 * 1024.0;

  std::ifstream meminfo("/proc/meminfo");
  std::string line;

  while (std::getline(meminfo, line)) {
    std::istringstream entry(line);
    std::string name;
    size_t value_KiB = 0;

    if (!(entry >> name >> value_KiB)) {
      continue;
    }

    if (name == "MemAvailable:") {
      constexpr double bytes_per_KiB = 1024.0;
      available_MiB = (value_KiB * bytes_per_KiB) / bytes_per_MiB;
      break;
    }
  }
#endif

  return available_MiB;
}

} // end namespace util
} // end namespace umpire
