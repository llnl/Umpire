!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
! Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
! project contributors. See the COPYRIGHT file for details.
!
! SPDX-License-Identifier: (MIT)
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

module umpire_fortran_version_tests

  use iso_c_binding
  use fruit
  use umpire_mod
  implicit none

  contains

      subroutine test_version
          use iso_c_binding

          integer(C_SIZE_T) :: major_ver, minor_ver, patch_ver

          major_ver = get_major_version()
          minor_ver = get_minor_version()
          patch_ver = get_patch_version()

          call assert_true(major_ver .eq. UMPIRE_VERSION_MAJOR)
          call assert_true(minor_ver .eq. UMPIRE_VERSION_MINOR)
          call assert_true(patch_ver .eq. UMPIRE_VERSION_PATCH)

      end subroutine test_version

end module umpire_fortran_version_tests

program fortran_test
  use fruit
  use umpire_fortran_version_tests

  implicit none
  logical ok

  call init_fruit

  call test_version

  call fruit_summary
  call fruit_finalize

  call is_all_successful(ok)
  if (.not. ok) then
    call exit(1)
  endif
end program fortran_test
