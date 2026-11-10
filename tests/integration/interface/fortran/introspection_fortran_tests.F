!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
! Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
! project contributors. See the COPYRIGHT file for details.
!
! SPDX-License-Identifier: (MIT)
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

module umpire_fortran_introspection_tests

  use iso_c_binding
  use fruit
  use umpire_mod
  implicit none

  contains

      subroutine test_introspection
          use iso_c_binding

          type(UmpireResourceManager) rm
          type(UmpireAllocator) allocator

          integer(C_INT), pointer, dimension(:) :: array
          integer(C_SIZE_T) :: memory_usage, memory_hwm, allocation_count
          integer(C_SIZE_T) :: zero

          zero = 0

          rm = rm%get_instance()
          allocator = rm%get_allocator_by_name("HOST")

          memory_usage = get_process_memory_usage()
          call assert_true(memory_usage .ge. zero)

          memory_hwm = get_process_memory_usage_hwm()
          call assert_true(memory_hwm .ge. zero)

          call allocator%allocate(array, [10])

          allocation_count = allocator%get_allocation_count()
          call assert_true(allocation_count .eq. 1)

          call allocator%deallocate(array)

          allocation_count = allocator%get_allocation_count()
          call assert_true(allocation_count .eq. 0)

          call allocator%delete()
      end subroutine test_introspection

end module umpire_fortran_introspection_tests

program fortran_test
  use fruit
  use umpire_fortran_introspection_tests

  implicit none
  logical ok

  call init_fruit

  call test_introspection

  call fruit_summary
  call fruit_finalize

  call is_all_successful(ok)
  if (.not. ok) then
    call exit(1)
  endif
end program fortran_test
