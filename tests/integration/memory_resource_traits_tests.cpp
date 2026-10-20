//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#include "gtest/gtest.h"
#include "umpire/Allocator.hpp"
#include "umpire/ResourceManager.hpp"
#include "umpire/strategy/QuickPool.hpp"
#include "umpire/util/MemoryResourceTraits.hpp"

//
// This test confirms that when an allocator is created with a specific
// memory resource, that same memory resource can be quiered and returned
//
class MemoryResourceTraitsTest : public ::testing::TestWithParam<std::string> {
 public:
  virtual void SetUp()
  {
    auto& rm = umpire::ResourceManager::getInstance();

    m_allocator = new umpire::Allocator(rm.getAllocator(GetParam()));
    m_allocator_pool =
        new umpire::Allocator(rm.makeAllocator<umpire::strategy::QuickPool>("pool_" + GetParam(), *m_allocator));

    m_resource = GetParam();
  }

  virtual void TearDown()
  {
    if (m_allocator)
      delete m_allocator;
    if (m_allocator_pool)
      delete m_allocator_pool;
  }

  umpire::Allocator* m_allocator{nullptr};
  umpire::Allocator* m_allocator_pool{nullptr};
  std::string m_resource;
};

umpire::MemoryResourceTraits::resource_type get_resource_trait(std::string resource)
{
  if (resource == "HOST")
    return umpire::MemoryResourceTraits::resource_type::host;
  else if (resource.find("::") != std::string::npos || resource == "DEVICE")
    return umpire::MemoryResourceTraits::resource_type::device;
  else if (resource == "DEVICE_CONST")
    return umpire::MemoryResourceTraits::resource_type::device_const;
  else if (resource == "UM")
    return umpire::MemoryResourceTraits::resource_type::um;
  else if (resource == "PINNED")
    return umpire::MemoryResourceTraits::resource_type::pinned;
  else if (resource == "FILE")
    return umpire::MemoryResourceTraits::resource_type::file;
  else
    return umpire::MemoryResourceTraits::resource_type::unknown;
}

TEST_P(MemoryResourceTraitsTest, ResourceTraitTest)
{
  umpire::MemoryResourceTraits::resource_type resource = get_resource_trait(m_resource);

  ASSERT_EQ(resource, m_allocator_pool->getAllocationStrategy()->getTraits().resource);
  ASSERT_EQ(resource, m_allocator->getAllocationStrategy()->getTraits().resource);
  ASSERT_EQ(m_allocator->getName(), m_resource);
}

// returns a vector of strings with the names of the
// memory resources currently available.
std::vector<std::string> memory_resource_strings()
{
  std::vector<std::string> resources;

  resources.push_back("HOST");
#if defined(UMPIRE_ENABLE_DEVICE)
  resources.push_back("DEVICE");
  auto& rm = umpire::ResourceManager::getInstance();
  for (int id = 1; id < rm.getNumDevices(); id++) {
    resources.push_back(std::string{"DEVICE::" + std::to_string(id)});
  }
#endif
#if defined(UMPIRE_ENABLE_UM)
  resources.push_back("UM");
#endif
#if defined(UMPIRE_ENABLE_CONST)
  resources.push_back("DEVICE_CONST");
#endif
#if defined(UMPIRE_ENABLE_PINNED)
  resources.push_back("PINNED");
#endif

  return resources;
}

INSTANTIATE_TEST_SUITE_P(MemoryResourceTraits, MemoryResourceTraitsTest,
                         ::testing::ValuesIn(memory_resource_strings()));
