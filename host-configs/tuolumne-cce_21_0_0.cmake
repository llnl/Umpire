#------------------------------------------------------------------------------
# !!!! This is a generated file, edit at own risk !!!!
#------------------------------------------------------------------------------
# CMake executable path: /usr/tce/packages/cmake/cmake-3.24.2/bin/cmake
#------------------------------------------------------------------------------

set(CMAKE_PREFIX_PATH "/usr/WS2/umdev/rad-bat/umpire/tuolumne/777755/cce_21_0_0/install/__spack_path_placeholder__/__spack_path_placeholder__/__spack_p/cce-21.0.0/blt-0.7.2-c52l3e6bkdrzzwbor275vdo3u62h2qup;/usr/WS2/umdev/rad-bat/umpire/tuolumne/777755/cce_21_0_0/install/__spack_path_placeholder__/__spack_path_placeholder__/__spack_p/cce-21.0.0/camp-2025.12.0-gdtqs3rixjb526n7dxhvmfw7apwetsnj;/usr/WS2/umdev/rad-bat/umpire/tuolumne/777755/cce_21_0_0/install/__spack_path_placeholder__/__spack_path_placeholder__/__spack_p/cce-21.0.0/fmt-12.1.0-py4p3d2xv3po3kflryc5jj7d6w65btch;/usr/WS2/umdev/rad-bat/umpire/tuolumne/777755/cce_21_0_0/install/__spack_path_placeholder__/__spack_path_placeholder__/__spack_p/none-none/compiler-wrapper-1.1.0-5jwq47pydjx26rsxpbvkpax3pce5jsby;/usr/tce/packages/cce-tce/cce-21.0.0;/usr/tce/packages/cmake/cmake-3.24.2" CACHE STRING "")

set(CMAKE_INSTALL_RPATH_USE_LINK_PATH "ON" CACHE STRING "")

set(CMAKE_BUILD_RPATH "/usr/WS2/umdev/rad-bat/umpire/tuolumne/777755/cce_21_0_0/install/__spack_path_placeholder__/__spack_path_placeholder__/__spack_p/cce-21.0.0/umpire-develop-s3xvf3wtg7ftqmg7subi4jcblouksypc/lib;/usr/WS2/umdev/rad-bat/umpire/tuolumne/777755/cce_21_0_0/install/__spack_path_placeholder__/__spack_path_placeholder__/__spack_p/cce-21.0.0/umpire-develop-s3xvf3wtg7ftqmg7subi4jcblouksypc/lib64;/opt/cray/pe/mpich/9.1.0/gtl/lib;/opt/cray/pe/cce/21.0.0/cce/x86_64/lib;/opt/rh/gcc-toolset-13/root/usr/lib/gcc/x86_64-redhat-linux/13" CACHE STRING "")

set(CMAKE_INSTALL_RPATH "/usr/WS2/umdev/rad-bat/umpire/tuolumne/777755/cce_21_0_0/install/__spack_path_placeholder__/__spack_path_placeholder__/__spack_p/cce-21.0.0/umpire-develop-s3xvf3wtg7ftqmg7subi4jcblouksypc/lib;/usr/WS2/umdev/rad-bat/umpire/tuolumne/777755/cce_21_0_0/install/__spack_path_placeholder__/__spack_path_placeholder__/__spack_p/cce-21.0.0/umpire-develop-s3xvf3wtg7ftqmg7subi4jcblouksypc/lib64;/opt/cray/pe/mpich/9.1.0/gtl/lib;/opt/cray/pe/cce/21.0.0/cce/x86_64/lib;/opt/rh/gcc-toolset-13/root/usr/lib/gcc/x86_64-redhat-linux/13" CACHE STRING "")

set(CMAKE_BUILD_TYPE "Release" CACHE STRING "")

#------------------------------------------------------------------------------
# Compilers
#------------------------------------------------------------------------------
# Compiler Spec: cce@21.0.0/3pdjz5dlk4pldwja4oaxv5q5fqlv2pp4
#------------------------------------------------------------------------------
if(DEFINED ENV{SPACK_CC})

  set(CMAKE_C_COMPILER "/usr/WS2/umdev/rad-bat/umpire/tuolumne/777755/cce_21_0_0/install/__spack_path_placeholder__/__spack_path_placeholder__/__spack_p/none-none/compiler-wrapper-1.1.0-5jwq47pydjx26rsxpbvkpax3pce5jsby/libexec/spack/cce/craycc" CACHE PATH "")

  set(CMAKE_CXX_COMPILER "/usr/WS2/umdev/rad-bat/umpire/tuolumne/777755/cce_21_0_0/install/__spack_path_placeholder__/__spack_path_placeholder__/__spack_p/none-none/compiler-wrapper-1.1.0-5jwq47pydjx26rsxpbvkpax3pce5jsby/libexec/spack/cce/case-insensitive/crayCC" CACHE PATH "")

  set(CMAKE_Fortran_COMPILER "/usr/WS2/umdev/rad-bat/umpire/tuolumne/777755/cce_21_0_0/install/__spack_path_placeholder__/__spack_path_placeholder__/__spack_p/none-none/compiler-wrapper-1.1.0-5jwq47pydjx26rsxpbvkpax3pce5jsby/libexec/spack/cce/crayftn" CACHE PATH "")

else()

  set(CMAKE_C_COMPILER "/usr/tce/packages/cce-tce/cce-21.0.0/bin/craycc" CACHE PATH "")

  set(CMAKE_CXX_COMPILER "/usr/tce/packages/cce-tce/cce-21.0.0/bin/crayCC" CACHE PATH "")

  set(CMAKE_Fortran_COMPILER "/usr/tce/packages/cce-tce/cce-21.0.0/bin/crayftn" CACHE PATH "")

endif()

set(ENABLE_FORTRAN ON CACHE BOOL "")

set(UMPIRE_ENABLE_C ON CACHE BOOL "")

set(BLT_EXE_LINKER_FLAGS "${BLT_EXE_LINKER_FLAGS} -Wl,-rpath,/opt/cray/pe/cce/21.0.0/cce-clang/x86_64/lib" CACHE STRING "Adds a missing rpath for libraries associated with the fortran compiler")

set(ENABLE_MPI OFF CACHE BOOL "")

set(UMPIRE_ENABLE_MPI3_SHARED_MEMORY OFF CACHE BOOL "")

#------------------------------------------------------------------------------
# Hardware
#------------------------------------------------------------------------------

#------------------------------------------------
# Package custom hardware settings
#------------------------------------------------

set(ENABLE_CUDA OFF CACHE BOOL "")

set(ENABLE_HIP OFF CACHE BOOL "")

set(UMPIRE_ENABLE_DEVICE_CONST OFF CACHE BOOL "")

set(UMPIRE_ENABLE_OPENMP_TARGET OFF CACHE BOOL "")

#------------------------------------------------------------------------------
# TPLs
#------------------------------------------------------------------------------

set(BLT_SOURCE_DIR "/usr/WS2/umdev/rad-bat/umpire/tuolumne/777755/cce_21_0_0/install/__spack_path_placeholder__/__spack_path_placeholder__/__spack_p/cce-21.0.0/blt-0.7.2-c52l3e6bkdrzzwbor275vdo3u62h2qup" CACHE PATH "")

set(camp_DIR "/usr/WS2/umdev/rad-bat/umpire/tuolumne/777755/cce_21_0_0/install/__spack_path_placeholder__/__spack_path_placeholder__/__spack_p/cce-21.0.0/camp-2025.12.0-gdtqs3rixjb526n7dxhvmfw7apwetsnj" CACHE PATH "")

set(fmt_DIR "/usr/WS2/umdev/rad-bat/umpire/tuolumne/777755/cce_21_0_0/install/__spack_path_placeholder__/__spack_path_placeholder__/__spack_p/cce-21.0.0/fmt-12.1.0-py4p3d2xv3po3kflryc5jj7d6w65btch" CACHE PATH "")

#------------------------------------------------------------------------------
# Build Options
#------------------------------------------------------------------------------

set(CMAKE_BUILD_TYPE "Release" CACHE STRING "")

set(BUILD_SHARED_LIBS OFF CACHE BOOL "")

set(ENABLE_WARNINGS_AS_ERRORS OFF CACHE BOOL "")

set(ENABLE_OPENMP OFF CACHE BOOL "")

set(ENABLE_EXAMPLES OFF CACHE BOOL "")

set(ENABLE_DOCS OFF CACHE BOOL "")

set(ENABLE_BENCHMARKS OFF CACHE BOOL "")

set(ENABLE_TESTS ON CACHE BOOL "")

set(UMPIRE_ENABLE_NUMA OFF CACHE BOOL "")

set(UMPIRE_ENABLE_DEVELOPER_BENCHMARKS OFF CACHE BOOL "")

set(UMPIRE_ENABLE_TOOLS OFF CACHE BOOL "")

set(UMPIRE_ENABLE_BACKTRACE OFF CACHE BOOL "")

set(UMPIRE_ENABLE_ASAN OFF CACHE BOOL "")

set(UMPIRE_ENABLE_SANITIZER_TESTS OFF CACHE BOOL "")

set(UMPIRE_ENABLE_DEVICE_ALLOCATOR OFF CACHE BOOL "")

set(UMPIRE_ENABLE_SQLITE_EXPERIMENTAL OFF CACHE BOOL "")

set(UMPIRE_ENABLE_IPC_SHARED_MEMORY OFF CACHE BOOL "")


