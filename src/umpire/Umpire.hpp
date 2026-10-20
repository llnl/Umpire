//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#ifndef UMPIRE_Umpire_HPP
#define UMPIRE_Umpire_HPP

#include <iostream>
#include <string>

#include "camp/camp.hpp"
#include "umpire/Allocator.hpp"
#include "umpire/ResourceManager.hpp"
#include "umpire/config.hpp"
#include "umpire/resource/MemoryResourceRegistry.hpp"
#include "umpire/util/AllocationRecord.hpp"
#include "umpire/util/MPI.hpp"
#include "umpire/util/io.hpp"

namespace umpire {

inline void initialize(
#if defined(UMPIRE_ENABLE_MPI)
    MPI_Comm umpire_communicator
#endif
)
{
  static bool initialized = false;

  if (!initialized) {
#if defined(UMPIRE_ENABLE_MPI)
    util::MPI::initialize(umpire_communicator);
#else
    util::MPI::initialize();
#endif

    initialized = true;
  }
}

void finalize();
/*!
 * \brief Allocate memory in the default space, with the default allocator.
 *
 * This method is a convenience wrapper around calls to the ResourceManager to
 * allocate memory in the default MemorySpace.
 *
 * \param size Number of bytes to allocate.
 */
inline void* malloc(std::size_t size)
{
  return ResourceManager::getInstance().getDefaultAllocator().allocate(size);
}

/*!
 * \brief Free any memory allocated with Umpire.
 *
 * This method is a convenience wrapper around calls to the ResourceManager, it
 * can be used to free allocations from any MemorySpace. *
 *
 * \param ptr Address to free.
 */
inline void free(void* ptr)
{
  return ResourceManager::getInstance().deallocate(ptr);
}

inline int get_major_version()
{
  return UMPIRE_VERSION_MAJOR;
}

inline int get_minor_version()
{
  return UMPIRE_VERSION_MINOR;
}

inline int get_patch_version()
{
  return UMPIRE_VERSION_PATCH;
}

inline std::string get_rc_version()
{
  return UMPIRE_VERSION_RC;
}

/*!
 * \brief Print the allocations from a specific allocator in a
 * human-readable format.
 *
 * \param allocator source Allocator.
 * \param os output stream
 */
void print_allocator_records(Allocator allocator, std::ostream& os = std::cout);

/*!
 * \brief Returns vector of AllocationRecords created by the allocator.
 *
 * \param allocator source Allocator.
 */
std::vector<util::AllocationRecord> get_allocator_records(Allocator allocator);

/*!
 * \brief Check whether the right allocation overlaps the left.
 *
 * right will overlap left if the right is greater than left, but less than
 * left+size, and right+size is strictly greater than left+size.
 *
 * \param left Pointer to left allocation
 * \param right Poniter to right allocation
 */
bool pointer_overlaps(void* left, void* right);

/*!
 * \brief Check whether the left allocation contains the right.
 *
 * right is contained by left if right is greater than left, and right+size is
 * greater than left+size.
 *
 * \param left Pointer to left allocation
 * \param right Poniter to right allocation
 */
bool pointer_contains(void* left, void* right);

/*!
 * \brief Check whether or not an Allocator is accessible from a given platform.
 *
 * This function describes which allocators should be accessible
 * from which CAMP platforms. Information on platform/allocator
 * accessibility can be found at
 * <https://umpire.readthedocs.io/en/develop/features/allocator_accessibility.html>
 *
 *\param camp::Platform p
 *\param umpire::Allocator a
 */
bool is_accessible(Platform p, Allocator a);

/*!
 * \brief Get the backtrace associated with the allocation of ptr
 *
 * The string may be empty if backtraces are not enabled.
 */
std::string get_backtrace(void* ptr);

/*!
 * \brief Get memory usage of the current process (uses underlying
 * system-dependent calls)
 */
std::size_t get_process_memory_usage();

/*!
 * \brief Get resident memory usage for memory mappings whose name contains
 * the provided substring.
 *
 * On Linux, this uses /proc/self/smaps and sums the RSS values for all mappings
 * whose name field contains the provided substring. The matching is case-sensitive
 * and uses substring matching (not exact matching), so providing "POSIX" will match
 * any mapping path containing "POSIX" anywhere in its name.
 *
 * Returns 0 on unsupported platforms or if no matching mappings are found.
 *
 * \param mapping_name Substring to search for in mapping names (case-sensitive)
 * \return Total RSS in bytes for all matching mappings, or 0 if none found
 */
std::size_t get_mapping_memory_usage(const std::string& mapping_name);

/*!
 * \brief Get high watermark memory usage of the current process (uses underlying
 * system-dependent calls)
 */
std::size_t get_process_memory_usage_hwm();

/*!
 * \brief Get the current memory usage for internal Umpire data structures.
 */
std::size_t get_internal_memory_usage();

/*!
 * \brief Mark an application-specific event string within Umpire life cycle.
 */
void mark_event(const std::string& event);

/*!
 * \brief Get the total umpire memory usage in bytes across all memory resources
 */
std::size_t get_total_bytes_allocated();

/*!
 * \brief Get memory usage of device device_id, using appropriate underlying
 * vendor API.
 */
std::size_t get_device_memory_usage(int device_id);

/*!
 * \brief Get all the leaked (active) allocations associated with allocator.
 */
std::vector<util::AllocationRecord> get_leaked_allocations(Allocator allocator);

/*!
 * \brief Return the default traits for the given allocator string
 */
umpire::MemoryResourceTraits get_default_resource_traits(const std::string& name);

/*!
 * \brief Return the pointer of an allocation for the given allocator and name
 */
void* find_pointer_from_name(Allocator allocator, const std::string& name);

#if defined(UMPIRE_ENABLE_MPI)
/*!
 * \brief Return the MPI communicator for a shared memory allocator.
 *
 * NOTE: Using this function will REQUIRE users to call the
 * cleanup_cached_communicators() function to avoid memory leaks.
 */
MPI_Comm get_communicator_for_allocator(Allocator a, MPI_Comm comm);
void cleanup_cached_communicators();
#endif

#if defined(UMPIRE_ENABLE_MPI)
/*!
 * \brief Collective preflight check for socket-scoped MPI3 shared memory.
 *
 * All ranks in \p comm must call this function.
 *
 * \param comm MPI communicator to check.
 * \param reason Failure reason when the check returns false.
 *
 * \return true when every rank in \p comm is bound to one socket.
 */
bool can_use_socket_scoped_mpi3_shared_memory(MPI_Comm comm, std::string& reason);
#endif

void register_external_allocation(void* ptr, util::AllocationRecord record);
util::AllocationRecord deregister_external_allocation(void* ptr);

/*!
 * \brief Returns the Camp resource associated with a particular allocation
 * **This function is used mainly for testing purposes.**
 *
 * \param Umpire allocator which was used to allocate the data
 * \param Pointer which was used for the allocation
 *
 * \return Camp resource associated with the allocation *assuming the Allocator
 * passed in is a ResourceAwarePool strategy and the allocation is either used or pending*
 */
camp::resources::Resource get_resource(Allocator a, void* ptr);

/*!
 * \brief Returns the number of pending chunks associated with a ResourceAwarePool Allocator
 * **This function is used mainly for testing purposes.**
 *
 * \param Umpire ResourceAwarePool allocator
 *
 * \return Number of currently pending chunks in the ResourceAwarePool
 */
std::size_t get_num_pending(Allocator a);

/*!
 * \brief Attempt to coalesce Allocator a, return true if a coalesce was performed.
 *
 * \return True if the Allocator was coalesced.
 */
bool try_coalesce(Allocator a);

/*!
 * \brief Attempt to coalesce Allocator a, throw if a does not support coalescing.
 *
 * \throw umpire::util::Exception if the Allocator doesn't support coalescing.
 */
void coalesce(Allocator a);

} // end of namespace umpire

#endif // UMPIRE_Umpire_HPP
