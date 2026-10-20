!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
!! Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
!! project contributors. See the COPYRIGHT file for details.
!!
!! SPDX-License-Identifier: (MIT)
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

module umpire_aligned_pool_fortran_tests
  use iso_c_binding
  use fruit
  use umpire_mod
  implicit none

contains

  subroutine test_list_pool_aligned
    use iso_c_binding
    implicit none

    type(UmpireResourceManager) rm
    type(UmpireAllocator) base_allocator
    type(UmpireAllocator) pool_allocator
    integer(C_SIZE_T) :: initial_size, block_size, alignment
    real(C_DOUBLE), pointer, dimension(:) :: array
    type(C_PTR) :: array_ptr
    integer(C_INTPTR_T) :: ptr_int

    rm = rm%get_instance()
    base_allocator = rm%get_allocator_by_name("HOST")

    ! Create aligned list pool with 64-byte alignment
    initial_size = 1024_8 * 1024_8
    block_size = 1024_8
    alignment = 64_8

    pool_allocator = rm%make_allocator_list_pool_aligned( &
        "fortran_aligned_list_pool", &
        base_allocator, &
        initial_size, &
        block_size, &
        alignment)

    ! Allocate an array
    call pool_allocator%allocate(array, [100])
    call assert_true(associated(array))

    ! Check alignment - convert pointer to integer and check modulo
    array_ptr = c_loc(array)
    ptr_int = transfer(array_ptr, ptr_int)
    call assert_equals(0, int(mod(ptr_int, int(alignment, C_INTPTR_T))), &
                       "Allocation should be aligned to 64 bytes")

    ! Deallocate
    call pool_allocator%deallocate(array)
    call assert_true(.not. associated(array))

    call pool_allocator%delete()
    call base_allocator%delete()
  end subroutine test_list_pool_aligned

  subroutine test_quick_pool_aligned
    use iso_c_binding
    implicit none

    type(UmpireResourceManager) rm
    type(UmpireAllocator) base_allocator
    type(UmpireAllocator) pool_allocator
    integer(C_SIZE_T) :: initial_size, block_size, alignment
    real(C_DOUBLE), pointer, dimension(:) :: array
    type(C_PTR) :: array_ptr
    integer(C_INTPTR_T) :: ptr_int

    rm = rm%get_instance()
    base_allocator = rm%get_allocator_by_name("HOST")

    ! Create aligned quick pool with 128-byte alignment
    initial_size = 1024_8 * 1024_8
    block_size = 1024_8
    alignment = 128_8

    pool_allocator = rm%make_allocator_quick_pool_aligned( &
        "fortran_aligned_quick_pool", &
        base_allocator, &
        initial_size, &
        block_size, &
        alignment)

    ! Allocate an array
    call pool_allocator%allocate(array, [100])
    call assert_true(associated(array))

    ! Check alignment
    array_ptr = c_loc(array)
    ptr_int = transfer(array_ptr, ptr_int)
    call assert_equals(0, int(mod(ptr_int, int(alignment, C_INTPTR_T))), &
                       "Allocation should be aligned to 128 bytes")

    ! Deallocate
    call pool_allocator%deallocate(array)
    call assert_true(.not. associated(array))

    call pool_allocator%delete()
    call base_allocator%delete()
  end subroutine test_quick_pool_aligned

  subroutine test_resource_aware_pool_aligned
    use iso_c_binding
    implicit none

    type(UmpireResourceManager) rm
    type(UmpireAllocator) base_allocator
    type(UmpireAllocator) pool_allocator
    integer(C_SIZE_T) :: initial_size, block_size, alignment
    real(C_DOUBLE), pointer, dimension(:) :: array
    type(C_PTR) :: array_ptr
    integer(C_INTPTR_T) :: ptr_int

    rm = rm%get_instance()
    base_allocator = rm%get_allocator_by_name("HOST")

    ! Create aligned resource-aware pool with 256-byte alignment
    initial_size = 1024_8 * 1024_8
    block_size = 1024_8
    alignment = 256_8

    pool_allocator = rm%make_allocator_resource_aware_pool_aligned( &
        "fortran_aligned_rap", &
        base_allocator, &
        initial_size, &
        block_size, &
        alignment)

    ! Allocate an array
    call pool_allocator%allocate(array, [100])
    call assert_true(associated(array))

    ! Check alignment
    array_ptr = c_loc(array)
    ptr_int = transfer(array_ptr, ptr_int)
    call assert_equals(0, int(mod(ptr_int, int(alignment, C_INTPTR_T))), &
                       "Allocation should be aligned to 256 bytes")

    ! Deallocate
    call pool_allocator%deallocate(array)
    call assert_true(.not. associated(array))

    call pool_allocator%delete()
    call base_allocator%delete()
  end subroutine test_resource_aware_pool_aligned

  subroutine test_list_pool_aligned_untracked
    use iso_c_binding
    implicit none

    type(UmpireResourceManager) rm
    type(UmpireAllocator) base_allocator
    type(UmpireAllocator) pool_allocator
    integer(C_SIZE_T) :: initial_size, block_size, alignment
    real(C_DOUBLE), pointer, dimension(:) :: array
    type(C_PTR) :: array_ptr
    integer(C_INTPTR_T) :: ptr_int

    rm = rm%get_instance()
    base_allocator = rm%get_allocator_by_name("HOST")

    ! Create untracked aligned list pool with 64-byte alignment
    initial_size = 1024_8 * 1024_8
    block_size = 1024_8
    alignment = 64_8

    pool_allocator = rm%make_allocator_list_pool_aligned_untracked( &
        "fortran_aligned_list_pool_untracked", &
        base_allocator, &
        initial_size, &
        block_size, &
        alignment)

    ! Allocate an array
    call pool_allocator%allocate(array, [100])
    call assert_true(associated(array))

    ! Check alignment
    array_ptr = c_loc(array)
    ptr_int = transfer(array_ptr, ptr_int)
    call assert_equals(0, int(mod(ptr_int, int(alignment, C_INTPTR_T))), &
                       "Untracked allocation should be aligned to 64 bytes")

    ! Deallocate
    call pool_allocator%deallocate(array)
    call assert_true(.not. associated(array))

    call pool_allocator%delete()
    call base_allocator%delete()
  end subroutine test_list_pool_aligned_untracked

  subroutine test_quick_pool_aligned_untracked
    use iso_c_binding
    implicit none

    type(UmpireResourceManager) rm
    type(UmpireAllocator) base_allocator
    type(UmpireAllocator) pool_allocator
    integer(C_SIZE_T) :: initial_size, block_size, alignment
    real(C_DOUBLE), pointer, dimension(:) :: array
    type(C_PTR) :: array_ptr
    integer(C_INTPTR_T) :: ptr_int

    rm = rm%get_instance()
    base_allocator = rm%get_allocator_by_name("HOST")

    ! Create untracked aligned quick pool with 128-byte alignment
    initial_size = 1024_8 * 1024_8
    block_size = 1024_8
    alignment = 128_8

    pool_allocator = rm%make_allocator_quick_pool_aligned_untracked( &
        "fortran_aligned_quick_pool_untracked", &
        base_allocator, &
        initial_size, &
        block_size, &
        alignment)

    ! Allocate an array
    call pool_allocator%allocate(array, [100])
    call assert_true(associated(array))

    ! Check alignment
    array_ptr = c_loc(array)
    ptr_int = transfer(array_ptr, ptr_int)
    call assert_equals(0, int(mod(ptr_int, int(alignment, C_INTPTR_T))), &
                       "Untracked allocation should be aligned to 128 bytes")

    ! Deallocate
    call pool_allocator%deallocate(array)
    call assert_true(.not. associated(array))

    call pool_allocator%delete()
    call base_allocator%delete()
  end subroutine test_quick_pool_aligned_untracked

  subroutine test_resource_aware_pool_aligned_untracked
    use iso_c_binding
    implicit none

    type(UmpireResourceManager) rm
    type(UmpireAllocator) base_allocator
    type(UmpireAllocator) pool_allocator
    integer(C_SIZE_T) :: initial_size, block_size, alignment
    real(C_DOUBLE), pointer, dimension(:) :: array
    type(C_PTR) :: array_ptr
    integer(C_INTPTR_T) :: ptr_int

    rm = rm%get_instance()
    base_allocator = rm%get_allocator_by_name("HOST")

    ! Create untracked aligned resource-aware pool with 256-byte alignment
    initial_size = 1024_8 * 1024_8
    block_size = 1024_8
    alignment = 256_8

    pool_allocator = rm%make_allocator_resource_aware_pool_aligned_untracked( &
        "fortran_aligned_rap_untracked", &
        base_allocator, &
        initial_size, &
        block_size, &
        alignment)

    ! Allocate an array
    call pool_allocator%allocate(array, [100])
    call assert_true(associated(array))

    ! Check alignment
    array_ptr = c_loc(array)
    ptr_int = transfer(array_ptr, ptr_int)
    call assert_equals(0, int(mod(ptr_int, int(alignment, C_INTPTR_T))), &
                       "Untracked allocation should be aligned to 256 bytes")

    ! Deallocate
    call pool_allocator%deallocate(array)
    call assert_true(.not. associated(array))

    call pool_allocator%delete()
    call base_allocator%delete()
  end subroutine test_resource_aware_pool_aligned_untracked

  subroutine test_multiple_allocations_aligned
    use iso_c_binding
    implicit none

    type(UmpireResourceManager) rm
    type(UmpireAllocator) base_allocator
    type(UmpireAllocator) pool_allocator
    integer(C_SIZE_T) :: initial_size, block_size, alignment
    real(C_DOUBLE), pointer, dimension(:) :: array1, array2, array3
    type(C_PTR) :: array_ptr
    integer(C_INTPTR_T) :: ptr_int

    rm = rm%get_instance()
    base_allocator = rm%get_allocator_by_name("HOST")

    ! Create aligned quick pool
    initial_size = 1024_8 * 1024_8 * 10_8
    block_size = 1024_8
    alignment = 64_8

    pool_allocator = rm%make_allocator_quick_pool_aligned( &
        "fortran_multi_aligned_pool", &
        base_allocator, &
        initial_size, &
        block_size, &
        alignment)

    ! Allocate multiple arrays and check each is aligned
    call pool_allocator%allocate(array1, [50])
    call assert_true(associated(array1))
    array_ptr = c_loc(array1)
    ptr_int = transfer(array_ptr, ptr_int)
    call assert_equals(0, int(mod(ptr_int, int(alignment, C_INTPTR_T))), &
                       "First allocation should be aligned")

    call pool_allocator%allocate(array2, [100])
    call assert_true(associated(array2))
    array_ptr = c_loc(array2)
    ptr_int = transfer(array_ptr, ptr_int)
    call assert_equals(0, int(mod(ptr_int, int(alignment, C_INTPTR_T))), &
                       "Second allocation should be aligned")

    call pool_allocator%allocate(array3, [200])
    call assert_true(associated(array3))
    array_ptr = c_loc(array3)
    ptr_int = transfer(array_ptr, ptr_int)
    call assert_equals(0, int(mod(ptr_int, int(alignment, C_INTPTR_T))), &
                       "Third allocation should be aligned")

    ! Deallocate all
    call pool_allocator%deallocate(array1)
    call pool_allocator%deallocate(array2)
    call pool_allocator%deallocate(array3)

    call pool_allocator%delete()
    call base_allocator%delete()
  end subroutine test_multiple_allocations_aligned

end module umpire_aligned_pool_fortran_tests

program fortran_test
  use fruit
  use umpire_aligned_pool_fortran_tests
  implicit none
  logical ok

  call init_fruit

  call test_list_pool_aligned
  call test_quick_pool_aligned
  call test_resource_aware_pool_aligned
  call test_list_pool_aligned_untracked
  call test_quick_pool_aligned_untracked
  call test_resource_aware_pool_aligned_untracked
  call test_multiple_allocations_aligned

  call fruit_summary
  call fruit_finalize

  call is_all_successful(ok)
  if (.not. ok) then
    call exit(1)
  endif
end program fortran_test
