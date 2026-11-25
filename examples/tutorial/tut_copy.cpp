//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#include "umpire/Allocator.hpp"
#include "umpire/ResourceManager.hpp"
#include "umpire/op.hpp"

void copy_data(double* source_data, std::size_t size, const std::string& destination)
{
  auto& rm = umpire::ResourceManager::getInstance();
  auto dest_allocator = rm.getAllocator(destination);

  double* dest_data = static_cast<double*>(dest_allocator.allocate(size * sizeof(double)));

  // _sphinx_tag_tut_copy_start
  umpire::copy(source_data, dest_data, size);
  // _sphinx_tag_tut_copy_end

  std::cout << "Copied source data (" << source_data << ") to destination " << destination << " (" << dest_data << ")"
            << std::endl;

  dest_allocator.deallocate(dest_data);
}

int main(int, char**)
{
  constexpr std::size_t SIZE = 1024;

  auto& rm = umpire::ResourceManager::getInstance();

  auto allocator = rm.getAllocator("HOST");

  double* data = static_cast<double*>(allocator.allocate(SIZE * sizeof(double)));

  std::cout << "Allocated " << (SIZE * sizeof(double)) << " bytes using the " << allocator.getName() << " allocator."
            << std::endl;

  std::cout << "Filling with 0.0...";

  for (std::size_t i = 0; i < SIZE; i++) {
    data[i] = 0.0;
  }

  std::cout << "done." << std::endl;

  copy_data(data, SIZE, "HOST");
#if defined(UMPIRE_ENABLE_DEVICE)
  copy_data(data, SIZE, "DEVICE");
#endif
#if defined(UMPIRE_ENABLE_UM)
  copy_data(data, SIZE, "UM");
#endif
#if defined(UMPIRE_ENABLE_PINNED)
  copy_data(data, SIZE, "PINNED");
#endif

  allocator.deallocate(data);

  return 0;
}
