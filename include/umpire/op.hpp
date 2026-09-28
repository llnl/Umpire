#pragma once

#include "umpire/config.hpp"
#include "umpire/op/host.hpp"
#include "umpire/op/operations.hpp"
#if defined(UMPIRE_ENABLE_CUDA)
#include "umpire/op/cuda.hpp"
#endif
#if defined(UMPIRE_ENABLE_HIP)
#include "umpire/op/hip.hpp"
#endif
#if defined(UMPIRE_ENABLE_SYCL)
#include "umpire/op/sycl.hpp"
#endif
#if defined(UMPIRE_ENABLE_OPENMP_TARGET)
#include "umpire/op/openmp_target.hpp"
#endif

#include "umpire/op/dispatch.hpp"
