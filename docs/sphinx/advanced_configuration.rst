.. _advanced_configuration:

======================
Advanced Configuration
======================

In addition to the normal options provided by CMake, Umpire uses some additional
configuration arguments to control optional features and behavior. Each
argument is a boolean option, and  can be turned on or off:

.. code-block:: bash

    -DENABLE_CUDA=Off

Here is a summary of the configuration options, their default value, and meaning:

    ====================================== ==========         ===========================================================================
    Variable                               Default            Meaning
    ====================================== ==========         ===========================================================================
    ``ENABLE_BENCHMARKS``                  On                 Build benchmark programs
    ``ENABLE_CUDA``                        Off                Enable CUDA support
    ``ENABLE_DOCS``                        Off                Build documentation (requires Sphinx and/or Doxygen)
    ``ENABLE_FORTRAN``                     Off                Build the Fortran API
    ``ENABLE_HIP``                         Off                Enable HIP support
    ``ENABLE_TESTS``                       On                 Build test executables
    ``UMPIRE_DISABLE_ALLOCATIONMAP_DEBUG`` Off                Disable verbose output from AllocationMap when a pointer cannot be found
    ``UMPIRE_ENABLE_ASAN``                 Off                Enable ASAN support
    ``UMPIRE_ENABLE_BACKTRACE_SYMBOLS``    Off                Enable symbol lookup for backtraces
    ``UMPIRE_ENABLE_BACKTRACE``            Off                Enable backtraces for allocations
    ``UMPIRE_ENABLE_C``                    Off                Build the C API
    ``UMPIRE_ENABLE_FILE_RESOURCE``        Off                Enable FILE support
    ``UMPIRE_ENABLE_INTROSPECTION_HEADER`` Off                Store introspection metadata in a header in front of each allocation
    ``UMPIRE_ENABLE_IPC_SHARED_MEMORY``    UMPIRE_ENABLE_MPI  Enable Shared Memory support
    ``UMPIRE_ENABLE_LOGGING``              On                 Enable Logging within Umpire
    ``UMPIRE_ENABLE_NUMA``                 Off                Enable NUMA support
    ``UMPIRE_ENABLE_PERFORMANCE_TESTS``    Off                Build and run performance tests
    ``UMPIRE_ENABLE_SLIC``                 Off                Enable SLIC logging
    ``UMPIRE_ENABLE_TOOLS``                Off                Enable tools like replay
    ====================================== ==========         ===========================================================================

These arguments are explained in more detail below:

* ``ENABLE_BENCHMARKS``
  This option will build the benchmark programs used to test performance.

* ``ENABLE_CUDA``
  This option enables support for NVIDIA GPUs using the CUDA programming model.
  If Umpire is built without CUDA or HIP support, then only the ``HOST``
  allocator is available for use.

* ``ENABLE_DOCS``
  Build user documentation (with Sphinx) and code documentation (with Doxygen)

* ``ENABLE_FORTRAN``
  Build the Fortran API.

* ``ENABLE_HIP``
  This option enables support for AMD GPUs using the ROCm stack and HIP
  programming model. If Umpire is built without CUDA or HIP support,
  then only the ``HOST`` allocator is available for use.

* ``ENABLE_TESTS``
  This option controls whether or not test executables will be built.

* ``UMPIRE_DISABLE_ALLOCATIONMAP_DEBUG``
  This option disables verbose output from the AllocationMap during debug
  builds. By default, when an allocation cannot be found, the contents of the
  AllocationMap are printed.

* ``UMPIRE_ENABLE_ASAN``
  This option enables address sanitization checks within Umpire by compilers
  that support options like ``-fsanitize=address``

* ``UMPIRE_ENABLE_BACKTRACE_SYMBOLS``
  This option enables symbol information to be provided with backtraces.  This
  requires -ldl to be specified for using programs.

* ``UMPIRE_ENABLE_BACKTRACE``
  This option enables collection of backtrace information for each allocation.

* ``UMPIRE_ENABLE_C``
  Build the C API, this allows accessing Umpire Allocators and the
  ResourceManager through a C interface.

* ``UMPIRE_ENABLE_FILE_RESOURCE``
  This option will allow the build to make all File Memory Allocation files.
  If Umpire is built without FILE, CUDA or HIP support, then only the ``HOST``
  allocator is available for use.

* ``UMPIRE_ENABLE_INTROSPECTION_HEADER``
  This option replaces the allocation map with a small header stored in front
  of each allocation, reducing the introspection overhead on the allocation
  path. All memory must be readable from the host, and some queries are not
  available; see :doc:`features/introspection_header`. Incompatible with
  ``UMPIRE_ENABLE_BACKTRACE``.

* ``UMPIRE_ENABLE_IPC_SHARED_MEMORY``
  This option enables support for interprocess shared memory.  Currently, this
  feature only exists for for ``HOST`` memory.

* ``UMPIRE_ENABLE_LOGGING``
  This option enables usage of Logging services for Umpire

* ``UMPIRE_ENABLE_NUMA``
  This option enables support for NUMA. The
  :class:`umpire::strategy::NumaPolicy` is available when built with this
  option, which may be used to locate the allocation to a specific node.

* ``UMPIRE_ENABLE_PERFORMANCE_TESTS``
  Build and run performance tests

* ``UMPIRE_ENABLE_SLIC``
  This option enables usage of logging services provided by SLIC.

* ``UMPIRE_ENABLE_TOOLS``
  Enable development tools for Umpire (replay, etc.)
