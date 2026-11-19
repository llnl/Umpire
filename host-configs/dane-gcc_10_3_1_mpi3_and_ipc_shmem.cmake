#------------------------------------------------------------------------------
# !!!! This is a generated file, edit at own risk !!!!
#------------------------------------------------------------------------------
# CMake executable path: /usr/tce/packages/cmake/cmake-3.25.2/bin/cmake
#------------------------------------------------------------------------------

set(CMAKE_PREFIX_PATH "/usr/WS2/umdev/rad-bat/umpire/dane/777756/gcc_10_3_1_mpi3_and_ipc_shmem/install/__spack_path_placeholder__/__spack_path_placehol/gcc-10.3.1/blt-0.7.2-henvjjga7hkb6yo5nkdj4ygbcrk34o33;/usr/WS2/umdev/rad-bat/umpire/dane/777756/gcc_10_3_1_mpi3_and_ipc_shmem/install/__spack_path_placeholder__/__spack_path_placehol/gcc-10.3.1/camp-2025.12.0-dxiat2vl74ggo2e6nlrq6fdqhuv7ek6l;/usr/WS2/umdev/rad-bat/umpire/dane/777756/gcc_10_3_1_mpi3_and_ipc_shmem/install/__spack_path_placeholder__/__spack_path_placehol/gcc-10.3.1/fmt-12.1.0-3hx5ekhcysgkgux2peegkquki7fz5h6t;/usr/WS2/umdev/rad-bat/umpire/dane/777756/gcc_10_3_1_mpi3_and_ipc_shmem/install/__spack_path_placeholder__/__spack_path_placehol/none-none/gcc-runtime-10.3.1-veatnatyxmu72cn4zinwosjrvip6xajg;/usr/WS2/umdev/rad-bat/umpire/dane/777756/gcc_10_3_1_mpi3_and_ipc_shmem/install/__spack_path_placeholder__/__spack_path_placehol/none-none/compiler-wrapper-1.1.0-pbd2ayub7z6lkuzpvgsnzfsycl3xbuy3;/usr/tce/packages/cmake/cmake-3.25.2;/usr/tce/packages/gcc/gcc-10.3.1;/usr/tce/packages/mvapich2/mvapich2-2.3.7-gcc-10.3.1" CACHE STRING "")

set(CMAKE_INSTALL_RPATH_USE_LINK_PATH "ON" CACHE STRING "")

set(CMAKE_BUILD_RPATH "/usr/WS2/umdev/rad-bat/umpire/dane/777756/gcc_10_3_1_mpi3_and_ipc_shmem/install/__spack_path_placeholder__/__spack_path_placehol/gcc-10.3.1/umpire-develop-46lkuxhfow3yehpdejthlo46netqzsdg/lib;/usr/WS2/umdev/rad-bat/umpire/dane/777756/gcc_10_3_1_mpi3_and_ipc_shmem/install/__spack_path_placeholder__/__spack_path_placehol/gcc-10.3.1/umpire-develop-46lkuxhfow3yehpdejthlo46netqzsdg/lib64;;/collab/usr/global/tools/tce4/packages/gcc/gcc-10.3.1/lib/gcc/x86_64-redhat-linux/10" CACHE STRING "")

set(CMAKE_INSTALL_RPATH "/usr/WS2/umdev/rad-bat/umpire/dane/777756/gcc_10_3_1_mpi3_and_ipc_shmem/install/__spack_path_placeholder__/__spack_path_placehol/gcc-10.3.1/umpire-develop-46lkuxhfow3yehpdejthlo46netqzsdg/lib;/usr/WS2/umdev/rad-bat/umpire/dane/777756/gcc_10_3_1_mpi3_and_ipc_shmem/install/__spack_path_placeholder__/__spack_path_placehol/gcc-10.3.1/umpire-develop-46lkuxhfow3yehpdejthlo46netqzsdg/lib64;;/collab/usr/global/tools/tce4/packages/gcc/gcc-10.3.1/lib/gcc/x86_64-redhat-linux/10" CACHE STRING "")

set(CMAKE_BUILD_TYPE "Release" CACHE STRING "")

#------------------------------------------------------------------------------
# Compilers
#------------------------------------------------------------------------------
# Compiler Spec: gcc@10.3.1/f32it42y27vxjpvysqkj2ezhbewn76p2
#------------------------------------------------------------------------------
if(DEFINED ENV{SPACK_CC})

  set(CMAKE_C_COMPILER "/usr/WS2/umdev/rad-bat/umpire/dane/777756/gcc_10_3_1_mpi3_and_ipc_shmem/install/__spack_path_placeholder__/__spack_path_placehol/none-none/compiler-wrapper-1.1.0-pbd2ayub7z6lkuzpvgsnzfsycl3xbuy3/libexec/spack/gcc/gcc" CACHE PATH "")

  set(CMAKE_CXX_COMPILER "/usr/WS2/umdev/rad-bat/umpire/dane/777756/gcc_10_3_1_mpi3_and_ipc_shmem/install/__spack_path_placeholder__/__spack_path_placehol/none-none/compiler-wrapper-1.1.0-pbd2ayub7z6lkuzpvgsnzfsycl3xbuy3/libexec/spack/gcc/g++" CACHE PATH "")

  # No Fortran compiler defined in spec
else()

  set(CMAKE_C_COMPILER "/usr/tce/packages/gcc/gcc-10.3.1/bin/gcc" CACHE PATH "")

  set(CMAKE_CXX_COMPILER "/usr/tce/packages/gcc/gcc-10.3.1/bin/g++" CACHE PATH "")

  # No Fortran compiler defined in spec
endif()

set(ENABLE_FORTRAN OFF CACHE BOOL "")

set(UMPIRE_ENABLE_C ON CACHE BOOL "")

#------------------------------------------------------------------------------
# MPI
#------------------------------------------------------------------------------

set(MPI_C_COMPILER "/usr/tce/packages/mvapich2/mvapich2-2.3.7-gcc-10.3.1/bin/mpicc" CACHE PATH "")

set(MPI_CXX_COMPILER "/usr/tce/packages/mvapich2/mvapich2-2.3.7-gcc-10.3.1/bin/mpicxx" CACHE PATH "")

set(MPI_Fortran_COMPILER "/usr/tce/packages/mvapich2/mvapich2-2.3.7-gcc-10.3.1/bin/mpif90" CACHE PATH "")

set(MPIEXEC_EXECUTABLE "/usr/bin/srun" CACHE PATH "")

set(MPIEXEC_NUMPROC_FLAG "-n" CACHE STRING "")

set(ENABLE_MPI ON CACHE BOOL "")

set(UMPIRE_ENABLE_MPI3_SHARED_MEMORY ON CACHE BOOL "")

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

set(BLT_SOURCE_DIR "/usr/WS2/umdev/rad-bat/umpire/dane/777756/gcc_10_3_1_mpi3_and_ipc_shmem/install/__spack_path_placeholder__/__spack_path_placehol/gcc-10.3.1/blt-0.7.2-henvjjga7hkb6yo5nkdj4ygbcrk34o33" CACHE PATH "")

set(camp_DIR "/usr/WS2/umdev/rad-bat/umpire/dane/777756/gcc_10_3_1_mpi3_and_ipc_shmem/install/__spack_path_placeholder__/__spack_path_placehol/gcc-10.3.1/camp-2025.12.0-dxiat2vl74ggo2e6nlrq6fdqhuv7ek6l" CACHE PATH "")

set(fmt_DIR "/usr/WS2/umdev/rad-bat/umpire/dane/777756/gcc_10_3_1_mpi3_and_ipc_shmem/install/__spack_path_placeholder__/__spack_path_placehol/gcc-10.3.1/fmt-12.1.0-3hx5ekhcysgkgux2peegkquki7fz5h6t" CACHE PATH "")

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

set(UMPIRE_ENABLE_IPC_SHARED_MEMORY ON CACHE BOOL "")


