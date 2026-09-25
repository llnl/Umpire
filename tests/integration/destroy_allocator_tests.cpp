//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#include "gtest/gtest.h"
#include "umpire/Allocator.hpp"
#include "umpire/ResourceManager.hpp"
#include "umpire/Umpire.hpp"
#include "umpire/config.hpp"
#include "umpire/strategy/QuickPool.hpp"

#if defined(UMPIRE_ENABLE_MPI)
  #include <mpi.h>
#endif

TEST(DestroyAllocatorTest, DestroyBasicQuickPool)
{
  auto& rm = umpire::ResourceManager::getInstance();

  // Create a QuickPool
  auto pool = rm.makeAllocator<umpire::strategy::QuickPool>("test_pool_basic", rm.getAllocator("HOST"));

  // Allocate and deallocate to verify it works
  void* ptr = pool.allocate(100);
  ASSERT_NE(nullptr, ptr);
  pool.deallocate(ptr);

  // Verify allocator exists
  ASSERT_TRUE(rm.isAllocator("test_pool_basic"));

  // Destroy the allocator
  ASSERT_NO_THROW(rm.destroyAllocator("test_pool_basic"));

  // Verify allocator no longer exists
  ASSERT_FALSE(rm.isAllocator("test_pool_basic"));

  // Verify getting the allocator throws an error
  ASSERT_THROW(rm.getAllocator("test_pool_basic"), umpire::runtime_error);
}

TEST(DestroyAllocatorTest, DestroyAllocatorById)
{
  auto& rm = umpire::ResourceManager::getInstance();

  // Create a QuickPool
  auto pool = rm.makeAllocator<umpire::strategy::QuickPool>("test_pool_by_id", rm.getAllocator("HOST"));
  int id = pool.getId();

  // Verify allocator exists
  ASSERT_TRUE(rm.isAllocator(id));

  // Destroy the allocator by ID
  ASSERT_NO_THROW(rm.destroyAllocator(id));

  // Verify allocator no longer exists
  ASSERT_FALSE(rm.isAllocator(id));
  ASSERT_FALSE(rm.isAllocator("test_pool_by_id"));
}

TEST(DestroyAllocatorTest, ErrorOnCoreResourceDestruction)
{
  auto& rm = umpire::ResourceManager::getInstance();

  // Attempt to destroy HOST allocator - should throw error
  ASSERT_THROW(rm.destroyAllocator("HOST"), umpire::runtime_error);

  // Verify HOST still exists
  ASSERT_TRUE(rm.isAllocator("HOST"));

#ifdef UMPIRE_ENABLE_DEVICE
  // Attempt to destroy DEVICE allocator - should throw error
  ASSERT_THROW(rm.destroyAllocator("DEVICE"), umpire::runtime_error);

  // Verify DEVICE still exists
  ASSERT_TRUE(rm.isAllocator("DEVICE"));
#endif
}

TEST(DestroyAllocatorTest, ErrorOnInternalAllocators)
{
  auto& rm = umpire::ResourceManager::getInstance();

  // Attempt to destroy internal null allocator - should throw error
  ASSERT_THROW(rm.destroyAllocator("__umpire_internal_null"), umpire::runtime_error);

  // Attempt to destroy internal 0-byte pool - should throw error
  ASSERT_THROW(rm.destroyAllocator("__umpire_internal_0_byte_pool"), umpire::runtime_error);
}

TEST(DestroyAllocatorTest, ActiveAllocationsNonStrictMode)
{
  auto& rm = umpire::ResourceManager::getInstance();
  auto pool = rm.makeAllocator<umpire::strategy::QuickPool>("test_pool_nonstrict", rm.getAllocator("HOST"));
  void* ptr = pool.allocate(100);
  ASSERT_NE(nullptr, ptr);

  // Destroy with active allocations - should succeed with warning in non-strict mode
  ASSERT_NO_THROW(rm.destroyAllocator("test_pool_nonstrict", false));

  // Verify allocator is destroyed
  ASSERT_FALSE(rm.isAllocator("test_pool_nonstrict"));

#if !defined(UMPIRE_ENABLE_INTROSPECTION_HEADER)
  // Allocation record is removed to avoid dangling allocator pointers
  // (with header-based introspection the leaked memory keeps its header, and
  // may itself have been freed by the pool, so it cannot be queried)
  ASSERT_FALSE(rm.hasAllocator(ptr));
#endif
}

TEST(DestroyAllocatorTest, FreeAllocationsOnDestroy)
{
  auto& rm = umpire::ResourceManager::getInstance();

  // Create a QuickPool
  auto pool = rm.makeAllocator<umpire::strategy::QuickPool>("test_pool_free", rm.getAllocator("HOST"));

  // Allocate multiple chunks of memory
  void* ptr1 = pool.allocate(100);
  void* ptr2 = pool.allocate(200);
  void* ptr3 = pool.allocate(300);
  ASSERT_NE(nullptr, ptr1);
  ASSERT_NE(nullptr, ptr2);
  ASSERT_NE(nullptr, ptr3);

#if defined(UMPIRE_ENABLE_INTROSPECTION_HEADER)
  // Allocations cannot be enumerated to free them
  ASSERT_THROW(rm.destroyAllocator("test_pool_free", true), umpire::runtime_error);

  pool.deallocate(ptr1);
  pool.deallocate(ptr2);
  pool.deallocate(ptr3);
  rm.destroyAllocator("test_pool_free", true);

  // Verify allocator is destroyed
  ASSERT_FALSE(rm.isAllocator("test_pool_free"));
#else
  // Destroy with free_allocations=true - should succeed and free all allocations
  rm.destroyAllocator("test_pool_free", true);

  // Verify allocator is destroyed
  ASSERT_FALSE(rm.isAllocator("test_pool_free"));

  // Allocation records are removed when allocations are freed
  ASSERT_FALSE(rm.hasAllocator(ptr1));
  ASSERT_FALSE(rm.hasAllocator(ptr2));
  ASSERT_FALSE(rm.hasAllocator(ptr3));
#endif
}

TEST(DestroyAllocatorTest, DestroyAllocatorWithAliases)
{
  auto& rm = umpire::ResourceManager::getInstance();

  // Create a QuickPool
  auto pool = rm.makeAllocator<umpire::strategy::QuickPool>("test_pool_alias", rm.getAllocator("HOST"));

  // Add aliases
  rm.addAlias("alias1", pool);
  rm.addAlias("alias2", pool);

  // Verify aliases exist
  ASSERT_TRUE(rm.isAllocator("test_pool_alias"));
  ASSERT_TRUE(rm.isAllocator("alias1"));
  ASSERT_TRUE(rm.isAllocator("alias2"));

  // Destroy by original name
  ASSERT_NO_THROW(rm.destroyAllocator("test_pool_alias"));

  // Verify all aliases are removed
  ASSERT_FALSE(rm.isAllocator("test_pool_alias"));
  ASSERT_FALSE(rm.isAllocator("alias1"));
  ASSERT_FALSE(rm.isAllocator("alias2"));
}

TEST(DestroyAllocatorTest, ParentChildWarningNonStrictMode)
{
  auto& rm = umpire::ResourceManager::getInstance();
  auto parent = rm.makeAllocator<umpire::strategy::QuickPool>("test_parent_nonstrict", rm.getAllocator("HOST"));
  rm.makeAllocator<umpire::strategy::QuickPool>("test_child_nonstrict", parent);

  // Destroy parent - should succeed with warning in non-strict mode
  ASSERT_NO_THROW(rm.destroyAllocator("test_parent_nonstrict"));

  // Clean up child
  rm.destroyAllocator("test_child_nonstrict");
}

#if defined(UMPIRE_ENABLE_IPC_SHARED_MEMORY) || defined(UMPIRE_ENABLE_MPI3_SHARED_MEMORY)
TEST(DestroyAllocatorTest, DestroySharedAllocator)
{
  auto& rm = umpire::ResourceManager::getInstance();

  // Create a SHARED allocator
  auto shared_alloc = rm.makeResource("SHARED");
  std::string shared_name = shared_alloc.getName();

  // Verify it's in the shared allocator names list
  auto shared_names = rm.getSharedAllocatorNames();
  ASSERT_TRUE(std::find(shared_names.begin(), shared_names.end(), shared_name) != shared_names.end());

  // Destroy the shared allocator
  ASSERT_NO_THROW(rm.destroyAllocator(shared_name));

  // Verify it's removed from shared allocator names
  shared_names = rm.getSharedAllocatorNames();
  ASSERT_TRUE(std::find(shared_names.begin(), shared_names.end(), shared_name) == shared_names.end());
}
#endif

TEST(DestroyAllocatorTest, AllocatorNotFound)
{
  auto& rm = umpire::ResourceManager::getInstance();

  // Attempt to destroy non-existent allocator - should throw error
  ASSERT_THROW(rm.destroyAllocator("non_existent_allocator"), umpire::runtime_error);
  ASSERT_THROW(rm.destroyAllocator(99999), umpire::runtime_error);
}

TEST(DestroyAllocatorTest, RepeatedDestroy)
{
  auto& rm = umpire::ResourceManager::getInstance();

  // Create a QuickPool
  rm.makeAllocator<umpire::strategy::QuickPool>("test_pool_repeated", rm.getAllocator("HOST"));

  // Destroy once - should succeed
  ASSERT_NO_THROW(rm.destroyAllocator("test_pool_repeated"));

  // Attempt to destroy again - should throw "not found" error
  ASSERT_THROW(rm.destroyAllocator("test_pool_repeated"), umpire::runtime_error);
}

TEST(DestroyAllocatorTest, DestroyAndRecreate)
{
  auto& rm = umpire::ResourceManager::getInstance();

  // Create a QuickPool
  auto pool1 = rm.makeAllocator<umpire::strategy::QuickPool>("test_pool_recreate", rm.getAllocator("HOST"));
  int id1 = pool1.getId();

  // Destroy it
  ASSERT_NO_THROW(rm.destroyAllocator("test_pool_recreate"));

  // Create a new allocator with the same name
  auto pool2 = rm.makeAllocator<umpire::strategy::QuickPool>("test_pool_recreate", rm.getAllocator("HOST"));
  int id2 = pool2.getId();

  // Should have different IDs
  ASSERT_NE(id1, id2);

  // Verify new allocator works
  void* ptr = pool2.allocate(100);
  ASSERT_NE(nullptr, ptr);
  pool2.deallocate(ptr);

  // Clean up
  rm.destroyAllocator("test_pool_recreate");
}

int main(int argc, char* argv[])
{
  int result = 0;

  ::testing::InitGoogleTest(&argc, argv);

#if defined(UMPIRE_ENABLE_MPI)
  MPI_Init(&argc, &argv);
#endif

  result = RUN_ALL_TESTS();

#if defined(UMPIRE_ENABLE_MPI)
  MPI_Finalize();
#endif

  return result;
}
