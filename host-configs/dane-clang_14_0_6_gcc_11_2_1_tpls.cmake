#------------------------------------------------------------------------------
# !!!! This is a generated file, edit at own risk !!!!
#------------------------------------------------------------------------------
# CMake executable path: /usr/tce/packages/cmake/cmake-3.25.2/bin/cmake
#------------------------------------------------------------------------------

set(CMAKE_PREFIX_PATH "/usr/WS2/umdev/rad-bat/umpire/dane/777756/clang_14_0_6_gcc_11_2_1_tpls/install/__spack_path_placeholder__/__spack_path_placehold/llvm-14.0.6/blt-0.7.2-yrtjkjinmpvyuoin56gq4h7wvdz44nwh;/usr/WS2/umdev/rad-bat/umpire/dane/777756/clang_14_0_6_gcc_11_2_1_tpls/install/__spack_path_placeholder__/__spack_path_placehold/llvm-14.0.6/camp-2025.12.0-sp3mlzf2qbgihilymxwuir4v7y3hjbw5;/usr/WS2/umdev/rad-bat/umpire/dane/777756/clang_14_0_6_gcc_11_2_1_tpls/install/__spack_path_placeholder__/__spack_path_placehold/llvm-14.0.6/fmt-12.1.0-mfrmdcedbz4o5diux226eml2epz54bqv;/usr/WS2/umdev/rad-bat/umpire/dane/777756/clang_14_0_6_gcc_11_2_1_tpls/install/__spack_path_placeholder__/__spack_path_placehold/none-none/gcc-runtime-11.2.1-7l5n2bq5frnxrmjm2m4a5zc5gdkows6w;/usr/WS2/umdev/rad-bat/umpire/dane/777756/clang_14_0_6_gcc_11_2_1_tpls/install/__spack_path_placeholder__/__spack_path_placehold/none-none/compiler-wrapper-1.1.0-pbd2ayub7z6lkuzpvgsnzfsycl3xbuy3;/usr/tce/packages/cmake/cmake-3.25.2;/usr/tce/packages/gcc/gcc-11.2.1;/usr/tce/packages/clang/clang-14.0.6" CACHE STRING "")

set(CMAKE_INSTALL_RPATH_USE_LINK_PATH "ON" CACHE STRING "")

set(CMAKE_BUILD_RPATH "/usr/WS2/umdev/rad-bat/umpire/dane/777756/clang_14_0_6_gcc_11_2_1_tpls/install/__spack_path_placeholder__/__spack_path_placehold/llvm-14.0.6/umpire-develop-fen7z2v5vawixairooon7i73xcn6agc3/lib;/usr/WS2/umdev/rad-bat/umpire/dane/777756/clang_14_0_6_gcc_11_2_1_tpls/install/__spack_path_placeholder__/__spack_path_placehold/llvm-14.0.6/umpire-develop-fen7z2v5vawixairooon7i73xcn6agc3/lib64;;/opt/rh/gcc-toolset-10/root/usr/lib/gcc/x86_64-redhat-linux/10;/collab/usr/global/tools/tce4/packages/gcc/gcc-11.2.1/lib/gcc/x86_64-redhat-linux/11" CACHE STRING "")

set(CMAKE_INSTALL_RPATH "/usr/WS2/umdev/rad-bat/umpire/dane/777756/clang_14_0_6_gcc_11_2_1_tpls/install/__spack_path_placeholder__/__spack_path_placehold/llvm-14.0.6/umpire-develop-fen7z2v5vawixairooon7i73xcn6agc3/lib;/usr/WS2/umdev/rad-bat/umpire/dane/777756/clang_14_0_6_gcc_11_2_1_tpls/install/__spack_path_placeholder__/__spack_path_placehold/llvm-14.0.6/umpire-develop-fen7z2v5vawixairooon7i73xcn6agc3/lib64;;/opt/rh/gcc-toolset-10/root/usr/lib/gcc/x86_64-redhat-linux/10;/collab/usr/global/tools/tce4/packages/gcc/gcc-11.2.1/lib/gcc/x86_64-redhat-linux/11" CACHE STRING "")

set(CMAKE_BUILD_TYPE "Release" CACHE STRING "")

#------------------------------------------------------------------------------
# Compilers
#------------------------------------------------------------------------------
# Compiler Spec: llvm@14.0.6/cc2ztr3uvowtkaese3ggrmbach5knhvc
#------------------------------------------------------------------------------
if(DEFINED ENV{SPACK_CC})

  set(CMAKE_C_COMPILER "/usr/WS2/umdev/rad-bat/umpire/dane/777756/clang_14_0_6_gcc_11_2_1_tpls/install/__spack_path_placeholder__/__spack_path_placehold/none-none/compiler-wrapper-1.1.0-pbd2ayub7z6lkuzpvgsnzfsycl3xbuy3/libexec/spack/clang/clang" CACHE PATH "")

  set(CMAKE_CXX_COMPILER "/usr/WS2/umdev/rad-bat/umpire/dane/777756/clang_14_0_6_gcc_11_2_1_tpls/install/__spack_path_placeholder__/__spack_path_placehold/none-none/compiler-wrapper-1.1.0-pbd2ayub7z6lkuzpvgsnzfsycl3xbuy3/libexec/spack/clang/clang++" CACHE PATH "")

  set(CMAKE_Fortran_COMPILER "/usr/WS2/umdev/rad-bat/umpire/dane/777756/clang_14_0_6_gcc_11_2_1_tpls/install/__spack_path_placeholder__/__spack_path_placehold/none-none/compiler-wrapper-1.1.0-pbd2ayub7z6lkuzpvgsnzfsycl3xbuy3/libexec/spack/gcc/gfortran" CACHE PATH "")

else()

  set(CMAKE_C_COMPILER "/usr/tce/packages/clang/clang-14.0.6/bin/clang" CACHE PATH "")

  set(CMAKE_CXX_COMPILER "/usr/tce/packages/clang/clang-14.0.6/bin/clang++" CACHE PATH "")

  set(CMAKE_Fortran_COMPILER "/usr/tce/packages/gcc/gcc-11.2.1/bin/gfortran" CACHE PATH "")

endif()

set(CMAKE_C_FLAGS "--gcc-toolchain=/usr/tce/packages/gcc/gcc-11.2.1" CACHE STRING "")

set(CMAKE_CXX_FLAGS "--gcc-toolchain=/usr/tce/packages/gcc/gcc-11.2.1" CACHE STRING "")

set(ENABLE_FORTRAN ON CACHE BOOL "")

set(UMPIRE_ENABLE_C ON CACHE BOOL "")

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

set(BLT_SOURCE_DIR "/usr/WS2/umdev/rad-bat/umpire/dane/777756/clang_14_0_6_gcc_11_2_1_tpls/install/__spack_path_placeholder__/__spack_path_placehold/llvm-14.0.6/blt-0.7.2-yrtjkjinmpvyuoin56gq4h7wvdz44nwh" CACHE PATH "")

set(camp_DIR "/usr/WS2/umdev/rad-bat/umpire/dane/777756/clang_14_0_6_gcc_11_2_1_tpls/install/__spack_path_placeholder__/__spack_path_placehold/llvm-14.0.6/camp-2025.12.0-sp3mlzf2qbgihilymxwuir4v7y3hjbw5" CACHE PATH "")

set(fmt_DIR "/usr/WS2/umdev/rad-bat/umpire/dane/777756/clang_14_0_6_gcc_11_2_1_tpls/install/__spack_path_placeholder__/__spack_path_placehold/llvm-14.0.6/fmt-12.1.0-mfrmdcedbz4o5diux226eml2epz54bqv" CACHE PATH "")

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

set(UMPIRE_ENABLE_TOOLS ON CACHE BOOL "")

set(UMPIRE_ENABLE_BACKTRACE OFF CACHE BOOL "")

set(UMPIRE_ENABLE_ASAN OFF CACHE BOOL "")

set(UMPIRE_ENABLE_SANITIZER_TESTS OFF CACHE BOOL "")

set(UMPIRE_ENABLE_DEVICE_ALLOCATOR OFF CACHE BOOL "")

set(UMPIRE_ENABLE_SQLITE_EXPERIMENTAL OFF CACHE BOOL "")

set(UMPIRE_ENABLE_IPC_SHARED_MEMORY OFF CACHE BOOL "")


