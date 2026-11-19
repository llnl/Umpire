#------------------------------------------------------------------------------
# !!!! This is a generated file, edit at own risk !!!!
#------------------------------------------------------------------------------
# CMake executable path: /usr/tce/packages/cmake/cmake-3.25.2/bin/cmake
#------------------------------------------------------------------------------

set(CMAKE_PREFIX_PATH "/usr/WS2/umdev/rad-bat/umpire/corona/777757/rocmcc_5_7_1_hip_openmp_device_alloc/install/__spack_path_placeholder__/__spack_path/intel-oneapi-compilers-classic-2021.6.0/blt-0.7.2-rtukr6clactmixyyxhnbpdnfqnpdbihd;/usr/WS2/umdev/rad-bat/umpire/corona/777757/rocmcc_5_7_1_hip_openmp_device_alloc/install/__spack_path_placeholder__/__spack_path/llvm-amdgpu-5.7.1/camp-2025.12.0-7wxfftk7mvro7rse2jgc63js5yov7cgq;/usr/WS2/umdev/rad-bat/umpire/corona/777757/rocmcc_5_7_1_hip_openmp_device_alloc/install/__spack_path_placeholder__/__spack_path/llvm-amdgpu-5.7.1/fmt-12.1.0-r4xlllbf6pplak2sloks73emb4hobnab;/usr/WS2/umdev/rad-bat/umpire/corona/777757/rocmcc_5_7_1_hip_openmp_device_alloc/install/__spack_path_placeholder__/__spack_path/none-none/compiler-wrapper-1.1.0-5jwq47pydjx26rsxpbvkpax3pce5jsby;/opt/rocm-5.7.1/hip;/opt/rocm-5.7.1;/usr/tce/packages/cmake/cmake-3.25.2;/usr/tce/packages/intel-classic/intel-classic-2021.6.0;/opt/rocm-5.7.1" CACHE STRING "")

set(CMAKE_INSTALL_RPATH_USE_LINK_PATH "ON" CACHE STRING "")

set(CMAKE_BUILD_RPATH "/usr/WS2/umdev/rad-bat/umpire/corona/777757/rocmcc_5_7_1_hip_openmp_device_alloc/install/__spack_path_placeholder__/__spack_path/llvm-amdgpu-5.7.1/umpire-develop-wiashi2icja3jihwchvgnyskn35e33jv/lib;/usr/WS2/umdev/rad-bat/umpire/corona/777757/rocmcc_5_7_1_hip_openmp_device_alloc/install/__spack_path_placeholder__/__spack_path/llvm-amdgpu-5.7.1/umpire-develop-wiashi2icja3jihwchvgnyskn35e33jv/lib64;/opt/rocm-5.7.1/lib;/opt/rocm-5.7.1/llvm/lib;/usr/tce/backend/installations/linux-rhel8-x86_64/gcc-10.3.1/intel-oneapi-compilers-2022.1.0-43xp3r52jx2q2rkf3ctzvskqu572xbky/compiler/2022.1.0/linux/compiler/lib/intel64_lin;/opt/rh/gcc-toolset-10/root/usr/lib/gcc/x86_64-redhat-linux/10" CACHE STRING "")

set(CMAKE_INSTALL_RPATH "/usr/WS2/umdev/rad-bat/umpire/corona/777757/rocmcc_5_7_1_hip_openmp_device_alloc/install/__spack_path_placeholder__/__spack_path/llvm-amdgpu-5.7.1/umpire-develop-wiashi2icja3jihwchvgnyskn35e33jv/lib;/usr/WS2/umdev/rad-bat/umpire/corona/777757/rocmcc_5_7_1_hip_openmp_device_alloc/install/__spack_path_placeholder__/__spack_path/llvm-amdgpu-5.7.1/umpire-develop-wiashi2icja3jihwchvgnyskn35e33jv/lib64;/opt/rocm-5.7.1/lib;/opt/rocm-5.7.1/llvm/lib;/usr/tce/backend/installations/linux-rhel8-x86_64/gcc-10.3.1/intel-oneapi-compilers-2022.1.0-43xp3r52jx2q2rkf3ctzvskqu572xbky/compiler/2022.1.0/linux/compiler/lib/intel64_lin;/opt/rh/gcc-toolset-10/root/usr/lib/gcc/x86_64-redhat-linux/10" CACHE STRING "")

set(CMAKE_BUILD_TYPE "Release" CACHE STRING "")

#------------------------------------------------------------------------------
# Compilers
#------------------------------------------------------------------------------
# Compiler Spec: llvm-amdgpu@5.7.1/5fccukxauy5t6p7xbsfk42k5fqflofix
#------------------------------------------------------------------------------
if(DEFINED ENV{SPACK_CC})

  set(CMAKE_C_COMPILER "/usr/WS2/umdev/rad-bat/umpire/corona/777757/rocmcc_5_7_1_hip_openmp_device_alloc/install/__spack_path_placeholder__/__spack_path/none-none/compiler-wrapper-1.1.0-5jwq47pydjx26rsxpbvkpax3pce5jsby/libexec/spack/rocmcc/amdclang" CACHE PATH "")

  set(CMAKE_CXX_COMPILER "/usr/WS2/umdev/rad-bat/umpire/corona/777757/rocmcc_5_7_1_hip_openmp_device_alloc/install/__spack_path_placeholder__/__spack_path/none-none/compiler-wrapper-1.1.0-5jwq47pydjx26rsxpbvkpax3pce5jsby/libexec/spack/rocmcc/amdclang++" CACHE PATH "")

  set(CMAKE_Fortran_COMPILER "/usr/WS2/umdev/rad-bat/umpire/corona/777757/rocmcc_5_7_1_hip_openmp_device_alloc/install/__spack_path_placeholder__/__spack_path/none-none/compiler-wrapper-1.1.0-5jwq47pydjx26rsxpbvkpax3pce5jsby/libexec/spack/intel/ifort" CACHE PATH "")

else()

  set(CMAKE_C_COMPILER "/opt/rocm-5.7.1/llvm/bin/amdclang" CACHE PATH "")

  set(CMAKE_CXX_COMPILER "/opt/rocm-5.7.1/llvm/bin/amdclang++" CACHE PATH "")

  set(CMAKE_Fortran_COMPILER "/usr/tce/packages/intel-classic/intel-classic-2021.6.0/bin/ifort" CACHE PATH "")

endif()

set(ENABLE_FORTRAN ON CACHE BOOL "")

set(UMPIRE_ENABLE_C ON CACHE BOOL "")

set(ENABLE_MPI OFF CACHE BOOL "")

set(UMPIRE_ENABLE_MPI3_SHARED_MEMORY OFF CACHE BOOL "")

#------------------------------------------------------------------------------
# Hardware
#------------------------------------------------------------------------------

#------------------------------------------------
# ROCm
#------------------------------------------------

set(ROCM_PATH "/opt/rocm-5.7.1" CACHE PATH "")

set(CMAKE_HIP_ARCHITECTURES "gfx906" CACHE STRING "")

set(CMAKE_HIP_COMPILER "/opt/rocm-5.7.1/bin/amdclang++" CACHE FILEPATH "")

#------------------------------------------------
# Package custom hardware settings
#------------------------------------------------

set(ENABLE_CUDA OFF CACHE BOOL "")

set(ENABLE_HIP ON CACHE BOOL "")

set(CMAKE_EXE_LINKER_FLAGS "-Wl,-rpath=/opt/rocm-5.7.1/llvm/lib/" CACHE STRING "")

set(UMPIRE_ENABLE_DEVICE_CONST OFF CACHE BOOL "")

set(UMPIRE_ENABLE_OPENMP_TARGET OFF CACHE BOOL "")

#------------------------------------------------------------------------------
# TPLs
#------------------------------------------------------------------------------

set(BLT_SOURCE_DIR "/usr/WS2/umdev/rad-bat/umpire/corona/777757/rocmcc_5_7_1_hip_openmp_device_alloc/install/__spack_path_placeholder__/__spack_path/intel-oneapi-compilers-classic-2021.6.0/blt-0.7.2-rtukr6clactmixyyxhnbpdnfqnpdbihd" CACHE PATH "")

set(camp_DIR "/usr/WS2/umdev/rad-bat/umpire/corona/777757/rocmcc_5_7_1_hip_openmp_device_alloc/install/__spack_path_placeholder__/__spack_path/llvm-amdgpu-5.7.1/camp-2025.12.0-7wxfftk7mvro7rse2jgc63js5yov7cgq" CACHE PATH "")

set(fmt_DIR "/usr/WS2/umdev/rad-bat/umpire/corona/777757/rocmcc_5_7_1_hip_openmp_device_alloc/install/__spack_path_placeholder__/__spack_path/llvm-amdgpu-5.7.1/fmt-12.1.0-r4xlllbf6pplak2sloks73emb4hobnab" CACHE PATH "")

#------------------------------------------------------------------------------
# Build Options
#------------------------------------------------------------------------------

set(CMAKE_BUILD_TYPE "Release" CACHE STRING "")

set(BUILD_SHARED_LIBS OFF CACHE BOOL "")

set(ENABLE_WARNINGS_AS_ERRORS OFF CACHE BOOL "")

set(ENABLE_OPENMP ON CACHE BOOL "")

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

set(UMPIRE_ENABLE_DEVICE_ALLOCATOR ON CACHE BOOL "")

set(UMPIRE_ENABLE_SQLITE_EXPERIMENTAL OFF CACHE BOOL "")

set(UMPIRE_ENABLE_IPC_SHARED_MEMORY OFF CACHE BOOL "")


