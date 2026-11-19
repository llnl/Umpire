#------------------------------------------------------------------------------
# !!!! This is a generated file, edit at own risk !!!!
#------------------------------------------------------------------------------
# CMake executable path: /usr/tce/packages/cmake/cmake-3.24.2/bin/cmake
#------------------------------------------------------------------------------

set(CMAKE_PREFIX_PATH "/usr/WS2/umdev/rad-bat/umpire/tioga/777758/rocmcc_6.4.3_hip_deviceconst/install/__spack_path_placeholder__/__spack_path_placehol/cce-21.0.0/blt-0.7.2-c52l3e6bkdrzzwbor275vdo3u62h2qup;/usr/WS2/umdev/rad-bat/umpire/tioga/777758/rocmcc_6.4.3_hip_deviceconst/install/__spack_path_placeholder__/__spack_path_placehol/llvm-amdgpu-6.4.3/camp-2025.12.0-23cozzyagvz6qiab3u3i2whdie7aly7c;/usr/WS2/umdev/rad-bat/umpire/tioga/777758/rocmcc_6.4.3_hip_deviceconst/install/__spack_path_placeholder__/__spack_path_placehol/llvm-amdgpu-6.4.3/fmt-12.1.0-f45xzbm4ejsbf4vhiudzv4zek2yebuno;/usr/WS2/umdev/rad-bat/umpire/tioga/777758/rocmcc_6.4.3_hip_deviceconst/install/__spack_path_placeholder__/__spack_path_placehol/none-none/compiler-wrapper-1.1.0-5jwq47pydjx26rsxpbvkpax3pce5jsby;/opt/rocm-6.4.3;/opt/rocm-6.4.3;/usr/tce/packages/cce-tce/cce-21.0.0;/usr/tce/packages/cmake/cmake-3.24.2;/opt/rocm-6.4.3" CACHE STRING "")

set(CMAKE_INSTALL_RPATH_USE_LINK_PATH "ON" CACHE STRING "")

set(CMAKE_BUILD_RPATH "/usr/WS2/umdev/rad-bat/umpire/tioga/777758/rocmcc_6.4.3_hip_deviceconst/install/__spack_path_placeholder__/__spack_path_placehol/llvm-amdgpu-6.4.3/umpire-develop-47yljjxgmbfgke3aqcuwwiadhn23j46l/lib;/usr/WS2/umdev/rad-bat/umpire/tioga/777758/rocmcc_6.4.3_hip_deviceconst/install/__spack_path_placeholder__/__spack_path_placehol/llvm-amdgpu-6.4.3/umpire-develop-47yljjxgmbfgke3aqcuwwiadhn23j46l/lib64;/opt/rocm-6.4.3/lib;/opt/rocm-6.4.3/llvm/lib;/opt/cray/pe/mpich/9.0.1/gtl/lib;/opt/cray/pe/mpich/9.1.0/gtl/lib;/opt/rh/gcc-toolset-13/root/usr/lib/gcc/x86_64-redhat-linux/13;/opt/cray/pe/cce/21.0.0/cce/x86_64/lib" CACHE STRING "")

set(CMAKE_INSTALL_RPATH "/usr/WS2/umdev/rad-bat/umpire/tioga/777758/rocmcc_6.4.3_hip_deviceconst/install/__spack_path_placeholder__/__spack_path_placehol/llvm-amdgpu-6.4.3/umpire-develop-47yljjxgmbfgke3aqcuwwiadhn23j46l/lib;/usr/WS2/umdev/rad-bat/umpire/tioga/777758/rocmcc_6.4.3_hip_deviceconst/install/__spack_path_placeholder__/__spack_path_placehol/llvm-amdgpu-6.4.3/umpire-develop-47yljjxgmbfgke3aqcuwwiadhn23j46l/lib64;/opt/rocm-6.4.3/lib;/opt/rocm-6.4.3/llvm/lib;/opt/cray/pe/mpich/9.0.1/gtl/lib;/opt/cray/pe/mpich/9.1.0/gtl/lib;/opt/rh/gcc-toolset-13/root/usr/lib/gcc/x86_64-redhat-linux/13;/opt/cray/pe/cce/21.0.0/cce/x86_64/lib" CACHE STRING "")

set(CMAKE_BUILD_TYPE "Release" CACHE STRING "")

#------------------------------------------------------------------------------
# Compilers
#------------------------------------------------------------------------------
# Compiler Spec: llvm-amdgpu@6.4.3/mshkqr4vmpibd5egqmxdvcbbafbkijbo
#------------------------------------------------------------------------------
if(DEFINED ENV{SPACK_CC})

  set(CMAKE_C_COMPILER "/usr/WS2/umdev/rad-bat/umpire/tioga/777758/rocmcc_6.4.3_hip_deviceconst/install/__spack_path_placeholder__/__spack_path_placehol/none-none/compiler-wrapper-1.1.0-5jwq47pydjx26rsxpbvkpax3pce5jsby/libexec/spack/rocmcc/amdclang" CACHE PATH "")

  set(CMAKE_CXX_COMPILER "/usr/WS2/umdev/rad-bat/umpire/tioga/777758/rocmcc_6.4.3_hip_deviceconst/install/__spack_path_placeholder__/__spack_path_placehol/none-none/compiler-wrapper-1.1.0-5jwq47pydjx26rsxpbvkpax3pce5jsby/libexec/spack/rocmcc/amdclang++" CACHE PATH "")

  set(CMAKE_Fortran_COMPILER "/usr/WS2/umdev/rad-bat/umpire/tioga/777758/rocmcc_6.4.3_hip_deviceconst/install/__spack_path_placeholder__/__spack_path_placehol/none-none/compiler-wrapper-1.1.0-5jwq47pydjx26rsxpbvkpax3pce5jsby/libexec/spack/cce/crayftn" CACHE PATH "")

else()

  set(CMAKE_C_COMPILER "/opt/rocm-6.4.3/llvm/bin/amdclang" CACHE PATH "")

  set(CMAKE_CXX_COMPILER "/opt/rocm-6.4.3/llvm/bin/amdclang++" CACHE PATH "")

  set(CMAKE_Fortran_COMPILER "/usr/tce/packages/cce-tce/cce-21.0.0/bin/crayftn" CACHE PATH "")

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

set(ROCM_PATH "/opt/rocm-6.4.3" CACHE PATH "")

set(CMAKE_HIP_ARCHITECTURES "gfx90a" CACHE STRING "")

set(CMAKE_HIP_COMPILER "/opt/rocm-6.4.3/bin/amdclang++" CACHE FILEPATH "")

#------------------------------------------------
# Package custom hardware settings
#------------------------------------------------

set(ENABLE_CUDA OFF CACHE BOOL "")

set(ENABLE_HIP ON CACHE BOOL "")

set(CMAKE_EXE_LINKER_FLAGS "-Wl,-rpath=/opt/rocm-6.4.3/llvm/lib/" CACHE STRING "")

set(UMPIRE_ENABLE_DEVICE_CONST ON CACHE BOOL "")

set(UMPIRE_ENABLE_OPENMP_TARGET OFF CACHE BOOL "")

#------------------------------------------------------------------------------
# TPLs
#------------------------------------------------------------------------------

set(BLT_SOURCE_DIR "/usr/WS2/umdev/rad-bat/umpire/tioga/777758/rocmcc_6.4.3_hip_deviceconst/install/__spack_path_placeholder__/__spack_path_placehol/cce-21.0.0/blt-0.7.2-c52l3e6bkdrzzwbor275vdo3u62h2qup" CACHE PATH "")

set(camp_DIR "/usr/WS2/umdev/rad-bat/umpire/tioga/777758/rocmcc_6.4.3_hip_deviceconst/install/__spack_path_placeholder__/__spack_path_placehol/llvm-amdgpu-6.4.3/camp-2025.12.0-23cozzyagvz6qiab3u3i2whdie7aly7c" CACHE PATH "")

set(fmt_DIR "/usr/WS2/umdev/rad-bat/umpire/tioga/777758/rocmcc_6.4.3_hip_deviceconst/install/__spack_path_placeholder__/__spack_path_placehol/llvm-amdgpu-6.4.3/fmt-12.1.0-f45xzbm4ejsbf4vhiudzv4zek2yebuno" CACHE PATH "")

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


