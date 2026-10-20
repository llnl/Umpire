//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////

#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>

#include <unistd.h>

#include "umpire/Allocator.hpp"
#include "umpire/ResourceManager.hpp"
#include "umpire/Umpire.hpp"
#include "umpire/util/node_memory.hpp"

namespace {
std::size_t page_size()
{
  long ps = ::sysconf(_SC_PAGESIZE);
  return (ps > 0) ? static_cast<std::size_t>(ps) : 4096;
}

std::string format_bytes(std::size_t bytes)
{
  constexpr double KiB = 1024.0;
  constexpr double MiB = 1024.0 * KiB;
  constexpr double GiB = 1024.0 * MiB;

  const double b = static_cast<double>(bytes);
  if (b >= GiB) {
    return std::to_string(b / GiB) + " GiB";
  } else if (b >= MiB) {
    return std::to_string(b / MiB) + " MiB";
  } else if (b >= KiB) {
    return std::to_string(b / KiB) + " KiB";
  } else {
    return std::to_string(bytes) + " B";
  }
}

void touch_one_byte_per_page(std::uint8_t* buffer, std::size_t bytes)
{
  const std::size_t ps = page_size();
  for (std::size_t i = 0; i < bytes; i += ps) {
    ++buffer[i];
  }
  if (bytes > 0) {
    ++buffer[bytes - 1];
  }
}

} // namespace

int main(int, char**)
{
  constexpr std::size_t segment_size = 512ULL * 1024ULL * 1024ULL;
  constexpr std::size_t alloc_size = 256ULL * 1024ULL * 1024ULL;

  auto& rm = umpire::ResourceManager::getInstance();
  auto traits = umpire::get_default_resource_traits("SHARED::POSIX");
  traits.size = segment_size;

  const std::string allocator_name = "SHARED::POSIX::release_example";
  umpire::Allocator allocator = rm.makeResource(allocator_name, traits);

  const double node_available_before_MiB = umpire::util::get_node_available_memory();
  if (node_available_before_MiB >= 0.0) {
    std::cout << std::fixed << std::setprecision(1)
              << "Node available memory before: " << node_available_before_MiB << " MiB\n";
  }

  const std::size_t rss_before = umpire::get_process_memory_usage();
  std::cout << "RSS before: " << format_bytes(rss_before) << "\n";

  const std::size_t shm_rss_before = umpire::get_mapping_memory_usage(allocator_name);
  if (shm_rss_before > 0) {
    std::cout << "Shared segment RSS before: " << format_bytes(shm_rss_before) << "\n";
  }

  void* ptr = nullptr;
  try {
    ptr = allocator.allocate("buffer", alloc_size);
  } catch (const std::exception& e) {
    std::cerr << "Failed to allocate " << format_bytes(alloc_size) << ": " << e.what() << "\n";
    return 1;
  }

  touch_one_byte_per_page(static_cast<std::uint8_t*>(ptr), alloc_size);

  const std::size_t rss_after_touch = umpire::get_process_memory_usage();
  std::cout << "RSS after touching allocation: " << format_bytes(rss_after_touch) << "\n";

  const std::size_t shm_rss_after_touch = umpire::get_mapping_memory_usage(allocator_name);
  if (shm_rss_after_touch > 0) {
    std::cout << "Shared segment RSS after touch: " << format_bytes(shm_rss_after_touch) << "\n";
  }

  allocator.deallocate(ptr);

  const std::size_t rss_after_free = umpire::get_process_memory_usage();
  std::cout << "RSS after deallocate (before release): " << format_bytes(rss_after_free) << "\n";

  const std::size_t shm_rss_after_free = umpire::get_mapping_memory_usage(allocator_name);
  if (shm_rss_after_free > 0) {
    std::cout << "Shared segment RSS after deallocate: " << format_bytes(shm_rss_after_free) << "\n";
  }

  allocator.release();

  const std::size_t rss_after_release = umpire::get_process_memory_usage();
  std::cout << "RSS after release: " << format_bytes(rss_after_release) << "\n";

  const double node_available_after_MiB = umpire::util::get_node_available_memory();
  if (node_available_before_MiB >= 0.0 && node_available_after_MiB >= 0.0) {
    std::cout << std::fixed << std::setprecision(1)
              << "Node available memory after:  " << node_available_after_MiB << " MiB\n"
              << "Node available delta:         " << (node_available_after_MiB - node_available_before_MiB)
              << " MiB\n";
  }

  const std::size_t shm_rss_after_release = umpire::get_mapping_memory_usage(allocator_name);
  if (shm_rss_after_release > 0) {
    std::cout << "Shared segment RSS after release: " << format_bytes(shm_rss_after_release) << "\n";
  } else {
    std::cout << "NOTE: shared segment RSS reporting requires Linux (/proc/self/smaps)\n";
  }

  return 0;
}
