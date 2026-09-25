# v2026.07.1

## Changes Impacting Builds

- Resolved errors related to exporting the legacy umpire CMake target.

# v2026.07.0

## Improvements

- A list of available allocators is output if the requested allocator is not found

## Changes Impacting Builds

- Updated the exported CMake target name to `umpire::umpire`. The legacy `umpire` target is deprecated, 
  but remains available when configured with UMPIRE_ENABLE_LEGACY_CMAKE_TARGETS=ON

- Umpire now requires C++20

## New Features

- Added a tryGetAllocator which can be used to return an optional `umpire::Allocator`

- Added an IPC Shared Memory Release function to return memory back to system

- Added a `destroyAllocator` method which frees any resources owned by that Allocator

- Added a MPI3 Shared Memory trait for socket scope

- Added a utility function that returns the available memory on node

## Bug Fixes

- Fixed a small memory leak affecting touched unused pages

# v2025.12.0

## Improvements

- Added `getInternalMemoryUsage` method to track Umpire's internal memory usage for metadata

- Improved/Optimized how the ResourceAwarePool handles Pending chunks

- Added Default Shared Memory resource clarifications in cmake and examples

- Added CUDA 13 Support

# v2025.09.0

## Changes Impacting Builds

This release of Umpire contains the following build requirement changes:

- Umpire now requires C++17 or later.

## New Features

- Added `DeviceIpcAllocator` for GPU memory sharing between processes.

- Added HIP support to `AllocationAdvisor`.

- Added HIP support for `get_device_mem_usage` function.

- Extended Fortran API support:
  - Added `AlignedAllocator` to Fortran API.
  - Added `SizeLimiter` to Fortran API.
  - Added other strategies to Fortran API.
  - Added logical allocation routines to Fortran API.
  - Extended FORTRAN allocation support to 7D.
  - Added 5D Fortran allocation routines.

- Allow both shared memory implementations to be used at the same time.

## Bug Fixes

- Fixed a communicator leak in `HostMpi3SharedMemoryResource`.

- Fixed handling of the CUDA language standard.

- Fixed HIP constant memory implementation.

# v2025.03.0

## New Features

- Added a ResourceAwarePool which allows memory from the same pool to be used (1) across multiple device streams and (2) in a single memory space environment without the potential to cause data races.

- Added MPI3 Shared Memory Allocators which uses MPI3 capabilities for Shared Memory.

- Added a `NamingShim` strategy to allow users to allocate IPC shared memory without providing a name.

## Bug Fixes

- Fixed a minor memory leak when using IPC Shared Memory Allocators.

## Improvements

- A `get_total_bytes_allocated` function was implemented which returns the total amount of bytes allocated with Umpire allocators.

- The `NamedAllocationStrategy` can now be used with IPC Shared Memory Allocators.

- Additional documentation for Shared Memory Allocators was created and reorganized.

- Additional documentation on requirements for Windows builds was added to the cmake.

# v2024.07.0

## Changes Impacting Builds

This release of Umpire contains new build requirements including:

- Cmake version 3.23 or later is required.

## Bug Fixes

- Umpire uses Fortran_FORMAT to avoid compilation errors when using LLVM flang.

## Improvements

- SYCL and Intel builds were added to Umpire`s DockerFile in addition to other builds in the Github workflow for better testing.

# v2024.02.1
## Bug Fixes
- Made fmt header-only by default allowing users to define preferred fmt target using UMPIRE_FMT_TARGET variable.

# v2024.02.0

## New Features

- Add `_hwm` variant of pool heuristic functions. These reallocate to the high watermark value after a coalescing, and can reduce memory overhead.

## Improvements

- Support 2023 OneAPI release.

- Add HIP support to the DeviceAllocator.

## Bug Fixes

- Prefix fmt macros to avoid conflicts with other libraries including fmt.

- Replace deprecated random_shuffle usage with shuffle for c++17.

- Make exported include directories relative to install prefix.

# v2022.10.0

## New Features

- New HIP Advise operations have been added for setting and unsetting of the
  `READ_MOSTLY`, `PREFERRED_LOCATION`, and `ACCESSED_BY` advice.

  For HIP versions >= 5, operations to set and unset `COARSE_GRAIN` has also been added.

- `getCurrentSize` and `getTotalSize` methods were added to the `DeviceAllocator` API.

- New event tracking has been added that can stream events to JSON (for replays)
  and SQLITE.

- `UMAP` allocation resource has been added.

## Improvements

- Using `try_get` for async device operations.

## Bug Fixes

- Fixed build problem for Fortan builds by properly installing the Umpire module files.

- Fixed build problem on certain configurations where the `std::filesystem` check was returning incorrect results.

- Instead of throwing an exception, the `is_device_allocator` helper function
  now returns `false` if the `DeviceAllocator` object has not yet been instantiated.

- Fixed `FixedMallocPool` on Windows, allowing the `QuickPool` allocator to be safely copied.

## Build Configuration Updates

- Add `UMPIRE_DISABLE_ALLOCATIONMAP_DEBUG` (default is `OFF`) option that allows users to disable
  the AllocationMap from dumping all records when an allocation is not found in a debug build.

- Using `UMPIRE_ENABLE_MPI` instead of `ENABLE_MPI` for Umpire-specific MPI capabilities

- Using `UMPIRE_ENABLE_IPC_SHARED_MEMORY` instead of `ENABLE_IPC_SHARED_MEMORY` to indicate this is an Umpire-specific implementation feature.

# v2022.03.1

This is a patch release of v2022.03 that fixes reported build errors by setting `UMPIRE_ENABLE_DOCS` back to OFF
by default since building documentation sets requires additional tools to build properly.

# v2022.03.0

## Changes Impacting Builds

This release of Umpire contains new build requirements including:

- C++14 is now required to build Umpire
- CMake version 3.14 or later is required

## Changes Impacting C/Fortran

- The CMake object library for `C/FORTRAN` interface has been reorganized.
  (**NOTE**: *This is a breaking change since the include paths are now different.*)

## New Interfaces

- Added a `getDeviceAllocator` function that allows users to get a `DeviceAllocator` object from the kernel without explicitly passing the allocator to the kernel first.
- Added a `reset` function to the `DeviceAllocator` so that old data can be rewritten.
- Expose `PREFETCH` operations registered with the `MemoryOperationRegistry` with a new `ResourceManager::prefetch` method.

## Removed Interfaces

The following functions previously marked as deprecated have now been removed:

- `DynamicPoolMap` and `DynamicPool` aliases removed
- `registerAllocator` and `isAllocatorRegistered` removed

## Fixes

- Fixed a cmake install config issue so that now users can find a package of Umpire with a version constraint.
- Fix `ResourceManager::isAllocator` to work for resources
- Fix comparison operators for `TypedAllocators`
- Fix host and device Allocator ID overlap
- Remove null and zero-byte pool from list of valid allocators

## New Configuration Options

- The `UMPIRE_ENABLE_DEVICE_ALLOCATOR` option was added to control whether or not the DeviceAllocator class is included in the library.  The default is "Off".

## Build/Deployment Improvements

- `C/FORTRAN` API is now auto generated
- The `umpire-config.cmake` package is now relocatable
- Use `blt` namespace for hip targets
- Umpire `CMakeList` options now have `UMPIRE_` prefixes and are now dependent upon corresponding `BLT` options.
- Removed hardcoded `-Xcompiler -mno-float128` for GCC 8+ with CUDA on PowerPC.
- Build Doxygen documentation on ReadTheDocs.

## Continuous Integration Updates

- Add CI job with interprocess shared memory and CUDA
- Add CI containers to allow for gcc{7,8,9}, clang{11,12}, and nvcc{10,11}
- Add CI to check pools work with `DEVICE_CONST` memory

# v6.0.0

Added documentation on allocator (in)accessibility as well as getAllocator usage.

Added a Release function to FixedPool and corresponding gtest in strategy_tests

Installed thirdparty exports in CMake configuration file

Replay will now display high water mark statistics per allocator.

Initial support for IPC Shared Memory via a "SHARED" resource allocator. IPC Shared memory is initially available on the Host resource and will default to the value of ENABLE_MPI.

Added get_communicator_for_allocator to get an MPI Communicator for the scope of a shared allocator.

Added Allocator::getStrategyName() to get name of the strategy used.

Added getActualHighwatermark to all pool strategies, returns the high water value of getActualSize.

Added umpire::mark_event() to mark an event during Umpire lifecycle

Added asynchronous memset and reallocate operations for CUDA and HIP.

Added support for named allocations.

DynamicPoolMap marked deprecated. QuickPool should be used instead.

Refactored pool coalesce heuristic API to return either 0 or the minimum pool size to allocate when a coalesce is to be performed. No functional change yet.

All asynchronous operations now return a camp::resources::EventProxy to avoid the overhead of creating Events when they are unused.

Removed all internal tracking, allocations are only tracked at the Allocator level.

# v5.0.1

- Fixed bug where zero-byte allocations from Umpire were sometimes incorrectly
  reported as not being Umpire allocations

# v5.0.0

- Memory Resource header and source files for HIP

- Unified Memory support for HIP, including testing and benchmarking (temp support for Fortran).

- Added a getParent functionality for retrieving the memory resource of an allocator.

- Changed enumeration names from all upper case to all lower case in order to
  avoid name collisions.

- Fixed up broken source links in tutorial documentation.

- registerAllocator is deprecated, addAlias should be used instead.

- Moved backend-specific resource code out of ResourceManager and into resource::MemoryResourceRegistry.

- Fixed accounting for number of releasable bytes in Quickpool that was causing
  coalesce operations to not work properly.

# v4.1.2

- Added workaround for incorrect nvcc compiler warning:
  "warning: missing return statement at end of non-void function"
  occuring in one Umpire's header files.

# v4.1.1

- Fixed DynamicPoolMap deallocate to make coalesce check O(1) again.

- Initialize m_default_allocator to HOST if not set explicitly.

# v4.1.0

- QuickPool available via the C & Fortran APIs.

- Resources are now created on-demand when accessed for the first time.

- Peer access is no longer automatically enabled for CUDA and HIP.

- Added cmake check to deterime if build subsystem capable of ASAN.

- Fixed ASAN poisoning to limit it to what user originally requested and not
  rounded amount.

- Improved resilliance of primary pool destructors so that giving back
  previously allocated blocks to a device that has already been cleaned up
  will no longer throw an error, but instead will now be logged and ignored.

# v4.0.1

- Fixed Umpire builds with MPI enabled

- Added missing wrapUmpire.hpp to installation directory

# v4.0.0

- Added a FILE memory resource that allocates memory using mmap'd files. This
  can be used to allocate memory from the burst buffers on machines like Sierra
  and Lassen.

- All pools now have an "alignment" parameter that can be provided to the
  constructor.

- MemoryResourceTraits now includes a `resource` member that can be used to
  indentify the underlying resource for any Allocator.

- Bundled tpl cxxopts has been replaced by CLI11 (only used when ENABLE_TOOLS=On)

- Fixed memory leaks in DynamicPoolList, QuickPool.

- Fixed reallocate operation when called on an allocation from a pool.

# v3.0.0

- Added support for multiple GPU devices, detected and registered as "DEVICE_N"
  where N is the device number.

- Added support for capturing function backtraces with allocations.

- Added `AlignedAllocator` to provide aligned allocations for host memory.

- Fixed builds using `-stdlib=c++`

- Switched to camp::Platform: Platform::cpu is now Platform::host

# v2.1.0

- Fixes a bug when calling reallocate with size 0.

- Replay tool now supports replaying reallocate operations.

# v2.0.0

- ENABLE_DEVICE_CONST CMake option to control whether device constant memory
  is enabled. It is now disabled by default.

- DeviceAllocator that provides a pool for allocations inside GPU kernels.

- Added "unset" operations for removing CUDA memory advice.

- Extended C/Fortran API with more allocation strategies.

- NamedAllocator that allows creating a new allocator that passes allocations
  through to underlying strategy

- UMPIRE_VERSION_X are now defined as macros, rather than constexpr variables

- Fixed reallocate to properly handle case where size == 0

- AllocationStrategy constructor parameters re-ordered for consistency

# v1.1.0

- Added symbol `umpire_ver_1_detected` to help detect version mismatches when
  linking multiple libraries that all use Umpire.

- Re-introduced pool algorithm used in pre-1.0.0 releases as `DynamicPoolList`,
  and renamed current strategy to `DynamicPoolMap`. `DynamicPool` is now an
  alias to `DynamicPoolMap`.

- Fix signature of C function `umpire_resourcemanager_make_allocator_pool` to
  take `size_t` not `int`.

- Restored `getActualSize` for all `Allocator` types
# v1.0.1

- Fixed a bug in DynamicPool where memory could be leaked when allocating a new
  block using the "minimum size" for an allocation smaller than the block.

# v1.0.0

- Umpire is MPI-aware (outputs rank information to logs and replays) when
  configured with the option ENABLE_MPI=On, and umpire::initialize(MPI_Comm
  comm) must be called.

- AllocationStrategies may be wrapped with multiple extra layers. To "unwrap" an
  Allocator to a specific strategy, the umpire::util::unwrap_allocator method
  can be used, for example:

  auto dynamic_pool =
    umpire::util::unwrap_allocator<umpire::strategy::DynamicPool>(allocator);

  This will impact users who have been using DynamicPool::coalesce. The cookbook
  recipe has been updated accordingly, and the previous code snippet can be
  used.

- Umpire now directs log and replay output to files, one per process. The
  filenames can be controlled by the environment variable UMPIRE_OUTPUT_BASENAME

- ENABLE_CUDA now set to Off by default.

- Allocations for 0 bytes now always return a valid pointer that cannot be read
  or written. These pointers can be deallocated.
