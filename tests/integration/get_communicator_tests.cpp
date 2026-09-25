//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////

#include <exception>
#include <string>

#include "gtest/gtest.h"
#include "mpi.h"
#include "umpire/ResourceManager.hpp"
#include "umpire/Umpire.hpp"
#include "umpire/config.hpp"
#include "umpire/strategy/NamedAllocationStrategy.hpp"
#include "umpire/strategy/NamingShim.hpp"

#if defined (UMPIRE_ENABLE_MPI3_SHARED_MEMORY)
const std::string trait_name = "SHARED::MPI3";
const std::string alloc_name = "SHARED::MPI3::allocator";
#elif defined (UMPIRE_ENABLE_IPC_SHARED_MEMORY)
const std::string trait_name = "SHARED::IPC";
const std::string alloc_name = "SHARED::IPC::allocator";
#endif

TEST(GetCommunicator, Null)
{
  auto& rm = umpire::ResourceManager::getInstance();
  auto allocator = rm.getAllocator("HOST");

  ASSERT_EQ(umpire::get_communicator_for_allocator(allocator, MPI_COMM_WORLD), MPI_COMM_NULL);
}

TEST(GetCommunicator, SharedAndCached)
{
  auto& rm = umpire::ResourceManager::getInstance();

  auto traits{umpire::get_default_resource_traits(trait_name)};
  traits.size = 4096;

  auto allocator = rm.makeResource(alloc_name, traits);

  auto comm = umpire::get_communicator_for_allocator(allocator, MPI_COMM_WORLD);
  ASSERT_NE(comm, MPI_COMM_NULL);

  auto cached_comm = umpire::get_communicator_for_allocator(allocator, MPI_COMM_WORLD);
  int result;
  MPI_Comm_compare(comm, cached_comm, &result);
  ASSERT_EQ(result, MPI_IDENT);
}

TEST(GetCommunicator, WrappedSharedAllocator)
{
  auto& rm = umpire::ResourceManager::getInstance();
  auto allocator = rm.getAllocator(alloc_name);

  auto comm = umpire::get_communicator_for_allocator(allocator, MPI_COMM_WORLD);
  ASSERT_NE(comm, MPI_COMM_NULL);

  // Wrapping a shared-memory allocator must not lose its communicator. The
  // wrapped communicator contains the same group of ranks as the base one
  // (MPI_IDENT when the same communicator object is returned, MPI_CONGRUENT
  // when an equivalent one is created).
  auto named_allocator =
      rm.makeAllocator<umpire::strategy::NamedAllocationStrategy>("named_" + alloc_name, allocator);
  auto named_comm = umpire::get_communicator_for_allocator(named_allocator, MPI_COMM_WORLD);
  ASSERT_NE(named_comm, MPI_COMM_NULL);

  int result{MPI_UNEQUAL};
  MPI_Comm_compare(comm, named_comm, &result);
  ASSERT_TRUE(result == MPI_IDENT || result == MPI_CONGRUENT);

  auto shim_allocator = rm.makeAllocator<umpire::strategy::NamingShim>("shim_" + alloc_name, allocator);
  auto shim_comm = umpire::get_communicator_for_allocator(shim_allocator, MPI_COMM_WORLD);
  ASSERT_NE(shim_comm, MPI_COMM_NULL);

  result = MPI_UNEQUAL;
  MPI_Comm_compare(comm, shim_comm, &result);
  ASSERT_TRUE(result == MPI_IDENT || result == MPI_CONGRUENT);
}

#if defined(__linux__) && defined(UMPIRE_ENABLE_MPI3_SHARED_MEMORY)
TEST(GetCommunicator, SharedSocket)
{
  auto& rm = umpire::ResourceManager::getInstance();

  auto traits{umpire::get_default_resource_traits("SHARED::MPI3")};
  traits.size = 4096;
  traits.scope = umpire::MemoryResourceTraits::shared_scope::socket;

  std::string reason;
  if (!umpire::can_use_socket_scoped_mpi3_shared_memory(MPI_COMM_WORLD, reason)) {
    GTEST_SKIP() << reason;
  }

  umpire::Allocator allocator;
  try {
    allocator = rm.makeResource("SHARED::MPI3::socket_allocator", traits);
  } catch (const std::exception& e) {
    GTEST_SKIP() << "Socket-scoped MPI3 shared memory requires ranks bound to a single socket (" << e.what() << ")";
  }

  auto comm = umpire::get_communicator_for_allocator(allocator, MPI_COMM_WORLD);
  ASSERT_NE(comm, MPI_COMM_NULL);

  auto repeated_comm = umpire::get_communicator_for_allocator(allocator, MPI_COMM_WORLD);
  int result{MPI_UNEQUAL};
  MPI_Comm_compare(comm, repeated_comm, &result);
  ASSERT_EQ(result, MPI_IDENT);
}
#endif

int main(int argc, char* argv[])
{
  int result = 0;

  ::testing::InitGoogleTest(&argc, argv);

  MPI_Init(&argc, &argv);

  result = RUN_ALL_TESTS();

  umpire::cleanup_cached_communicators();

  MPI_Finalize();

  return result;
}
