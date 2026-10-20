//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
//
// Shroud splicer overrides for C wrappers.
//
// These bufferify wrappers correspond to explicit non-type template
// instantiations such as makeAllocator<Strategy, false>(...). Shroud emits
// invalid member calls for these overloads in the generated bufferify helpers,
// so provide the exact wrapper bodies here.

// splicer begin class.ResourceManager.method.make_allocator_list_pool_untracked_bufferify
const std::string SHCXX_name(name, Lname);
umpire::Allocator * SHCXX_allocator =
    static_cast<umpire::Allocator *>(allocator.addr);
umpire::Allocator * SHCXX_rv = new umpire::Allocator;
*SHCXX_rv = SH_this->makeAllocator<umpire::strategy::DynamicPoolList, false>(
    SHCXX_name, *SHCXX_allocator, initial_size, block);
SHC_rv->addr = SHCXX_rv;
SHC_rv->idtor = 1;
return SHC_rv;
// splicer end class.ResourceManager.method.make_allocator_list_pool_untracked_bufferify

// splicer begin class.ResourceManager.method.make_allocator_quick_pool_untracked_bufferify
const std::string SHCXX_name(name, Lname);
umpire::Allocator * SHCXX_allocator =
    static_cast<umpire::Allocator *>(allocator.addr);
umpire::Allocator * SHCXX_rv = new umpire::Allocator;
*SHCXX_rv = SH_this->makeAllocator<umpire::strategy::QuickPool, false>(
    SHCXX_name, *SHCXX_allocator, initial_size, block);
SHC_rv->addr = SHCXX_rv;
SHC_rv->idtor = 1;
return SHC_rv;
// splicer end class.ResourceManager.method.make_allocator_quick_pool_untracked_bufferify

// splicer begin class.ResourceManager.method.make_allocator_resource_aware_pool_untracked_bufferify
const std::string SHCXX_name(name, Lname);
umpire::Allocator * SHCXX_allocator =
    static_cast<umpire::Allocator *>(allocator.addr);
umpire::Allocator * SHCXX_rv = new umpire::Allocator;
*SHCXX_rv = SH_this->makeAllocator<umpire::strategy::ResourceAwarePool, false>(
    SHCXX_name, *SHCXX_allocator, initial_size, block);
SHC_rv->addr = SHCXX_rv;
SHC_rv->idtor = 1;
return SHC_rv;
// splicer end class.ResourceManager.method.make_allocator_resource_aware_pool_untracked_bufferify

// splicer begin class.ResourceManager.method.make_allocator_list_pool_aligned_bufferify
const std::string SHCXX_name(name, Lname);
umpire::Allocator * SHCXX_allocator =
    static_cast<umpire::Allocator *>(allocator.addr);
umpire::Allocator * SHCXX_rv = new umpire::Allocator;
*SHCXX_rv = SH_this->makeAllocator<umpire::strategy::DynamicPoolList>(
    SHCXX_name, *SHCXX_allocator, initial_size, block, alignment);
SHC_rv->addr = SHCXX_rv;
SHC_rv->idtor = 1;
return SHC_rv;
// splicer end class.ResourceManager.method.make_allocator_list_pool_aligned_bufferify

// splicer begin class.ResourceManager.method.make_allocator_quick_pool_aligned_bufferify
const std::string SHCXX_name(name, Lname);
umpire::Allocator * SHCXX_allocator =
    static_cast<umpire::Allocator *>(allocator.addr);
umpire::Allocator * SHCXX_rv = new umpire::Allocator;
*SHCXX_rv = SH_this->makeAllocator<umpire::strategy::QuickPool>(
    SHCXX_name, *SHCXX_allocator, initial_size, block, alignment);
SHC_rv->addr = SHCXX_rv;
SHC_rv->idtor = 1;
return SHC_rv;
// splicer end class.ResourceManager.method.make_allocator_quick_pool_aligned_bufferify

// splicer begin class.ResourceManager.method.make_allocator_resource_aware_pool_aligned_bufferify
const std::string SHCXX_name(name, Lname);
umpire::Allocator * SHCXX_allocator =
    static_cast<umpire::Allocator *>(allocator.addr);
umpire::Allocator * SHCXX_rv = new umpire::Allocator;
*SHCXX_rv = SH_this->makeAllocator<umpire::strategy::ResourceAwarePool>(
    SHCXX_name, *SHCXX_allocator, initial_size, block, alignment);
SHC_rv->addr = SHCXX_rv;
SHC_rv->idtor = 1;
return SHC_rv;
// splicer end class.ResourceManager.method.make_allocator_resource_aware_pool_aligned_bufferify

// splicer begin class.ResourceManager.method.make_allocator_list_pool_aligned_untracked_bufferify
const std::string SHCXX_name(name, Lname);
umpire::Allocator * SHCXX_allocator =
    static_cast<umpire::Allocator *>(allocator.addr);
umpire::Allocator * SHCXX_rv = new umpire::Allocator;
*SHCXX_rv = SH_this->makeAllocator<umpire::strategy::DynamicPoolList, false>(
    SHCXX_name, *SHCXX_allocator, initial_size, block, alignment);
SHC_rv->addr = SHCXX_rv;
SHC_rv->idtor = 1;
return SHC_rv;
// splicer end class.ResourceManager.method.make_allocator_list_pool_aligned_untracked_bufferify

// splicer begin class.ResourceManager.method.make_allocator_quick_pool_aligned_untracked_bufferify
const std::string SHCXX_name(name, Lname);
umpire::Allocator * SHCXX_allocator =
    static_cast<umpire::Allocator *>(allocator.addr);
umpire::Allocator * SHCXX_rv = new umpire::Allocator;
*SHCXX_rv = SH_this->makeAllocator<umpire::strategy::QuickPool, false>(
    SHCXX_name, *SHCXX_allocator, initial_size, block, alignment);
SHC_rv->addr = SHCXX_rv;
SHC_rv->idtor = 1;
return SHC_rv;
// splicer end class.ResourceManager.method.make_allocator_quick_pool_aligned_untracked_bufferify

// splicer begin class.ResourceManager.method.make_allocator_resource_aware_pool_aligned_untracked_bufferify
const std::string SHCXX_name(name, Lname);
umpire::Allocator * SHCXX_allocator =
    static_cast<umpire::Allocator *>(allocator.addr);
umpire::Allocator * SHCXX_rv = new umpire::Allocator;
*SHCXX_rv = SH_this->makeAllocator<umpire::strategy::ResourceAwarePool, false>(
    SHCXX_name, *SHCXX_allocator, initial_size, block, alignment);
SHC_rv->addr = SHCXX_rv;
SHC_rv->idtor = 1;
return SHC_rv;
// splicer end class.ResourceManager.method.make_allocator_resource_aware_pool_aligned_untracked_bufferify

// splicer begin class.ResourceManager.method.make_allocator_fixed_pool_untracked_bufferify
const std::string SHCXX_name(name, Lname);
umpire::Allocator * SHCXX_allocator =
    static_cast<umpire::Allocator *>(allocator.addr);
umpire::Allocator * SHCXX_rv = new umpire::Allocator;
*SHCXX_rv = SH_this->makeAllocator<umpire::strategy::FixedPool, false>(
    SHCXX_name, *SHCXX_allocator, object_size);
SHC_rv->addr = SHCXX_rv;
SHC_rv->idtor = 1;
return SHC_rv;
// splicer end class.ResourceManager.method.make_allocator_fixed_pool_untracked_bufferify

// splicer begin class.ResourceManager.method.make_allocator_monotonic_untracked_bufferify
const std::string SHCXX_name(name, Lname);
umpire::Allocator * SHCXX_allocator =
    static_cast<umpire::Allocator *>(allocator.addr);
umpire::Allocator * SHCXX_rv = new umpire::Allocator;
*SHCXX_rv =
    SH_this->makeAllocator<umpire::strategy::MonotonicAllocationStrategy, false>(
        SHCXX_name, *SHCXX_allocator, object_size);
SHC_rv->addr = SHCXX_rv;
SHC_rv->idtor = 1;
return SHC_rv;
// splicer end class.ResourceManager.method.make_allocator_monotonic_untracked_bufferify

// splicer begin class.ResourceManager.method.make_allocator_slot_pool_untracked_bufferify
const std::string SHCXX_name(name, Lname);
umpire::Allocator * SHCXX_allocator =
    static_cast<umpire::Allocator *>(allocator.addr);
umpire::Allocator * SHCXX_rv = new umpire::Allocator;
*SHCXX_rv = SH_this->makeAllocator<umpire::strategy::SlotPool, false>(
    SHCXX_name, *SHCXX_allocator, object_size);
SHC_rv->addr = SHCXX_rv;
SHC_rv->idtor = 1;
return SHC_rv;
// splicer end class.ResourceManager.method.make_allocator_slot_pool_untracked_bufferify

// splicer begin class.ResourceManager.method.make_allocator_mixed_pool_untracked_bufferify
const std::string SHCXX_name(name, Lname);
umpire::Allocator * SHCXX_allocator =
    static_cast<umpire::Allocator *>(allocator.addr);
umpire::Allocator * SHCXX_rv = new umpire::Allocator;
*SHCXX_rv = SH_this->makeAllocator<umpire::strategy::MixedPool, false>(
    SHCXX_name, *SHCXX_allocator, smallest_fixed_obj_size,
    largest_fixed_obj_size, max_initial_fixed_pool_size,
    fixed_size_multiplier, quick_pool_initial_alloc_size,
    quick_pool_min_alloc_size, quick_pool_align_bytes);
SHC_rv->addr = SHCXX_rv;
SHC_rv->idtor = 1;
return SHC_rv;
// splicer end class.ResourceManager.method.make_allocator_mixed_pool_untracked_bufferify
