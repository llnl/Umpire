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
    ``UMPIRE_ENABLE_IPC_SHARED_MEMORY``    UMPIRE_ENABLE_MPI  Enable Shared Memory support
    ``UMPIRE_ENABLE_LOGGING``              On                 Enable Logging within Umpire
    ``UMPIRE_ENABLE_NUMA``                 Off                Enable NUMA support
    ``UMPIRE_ENABLE_PERFORMANCE_TESTS``    Off                Build and run performance tests
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

* ``UMPIRE_ENABLE_IPC_SHARED_MEMORY``
  This option enables support for interprocess shared memory.  Currently, this
  feature only exists for for ``HOST`` memory.

* ``UMPIRE_ENABLE_LOGGING``
  This option enables usage of Logging services for Umpire. Umpire uses the
  spdlog library for logging. When enabled, logging behavior can be controlled
  via environment variables at runtime (see :ref:`logging_configuration` below).

* ``UMPIRE_ENABLE_NUMA``
  This option enables support for NUMA. The
  :class:`umpire::strategy::NumaPolicy` is available when built with this
  option, which may be used to locate the allocation to a specific node.

* ``UMPIRE_ENABLE_PERFORMANCE_TESTS``
  Build and run performance tests

* ``UMPIRE_ENABLE_TOOLS``
  Enable development tools for Umpire (replay, etc.)

.. _logging_configuration:

Logging Configuration
=====================

When Umpire is built with ``UMPIRE_ENABLE_LOGGING=On`` (the default), logging
behavior can be controlled at runtime using environment variables. Umpire uses
the spdlog library for high-performance, thread-safe logging.

Environment Variables
---------------------

The following environment variables control Umpire's logging behavior:

* ``UMPIRE_LOG_LEVEL``

  Controls which log messages are output. Must be set to enable logging.

  Valid values (case-insensitive):

  - ``ERROR`` - Only error messages
  - ``WARNING`` - Warnings and errors
  - ``INFO`` - Informational messages, warnings, and errors (recommended for debugging)
  - ``DEBUG`` - All messages including detailed debug information

  If not set, logging is completely disabled. Example:

  .. code-block:: bash

      export UMPIRE_LOG_LEVEL=INFO

* ``UMPIRE_LOG_TO_CONSOLE``

  Controls whether log messages are written to the console (stderr) in addition
  to the log file. Default is ``off``: log messages are written to the log
  file only, matching the behavior of previous Umpire versions.

  Valid values (case-insensitive):

  - ``1``, ``true``, ``on`` - Enable console output
  - ``0``, ``false``, ``off`` - Disable console output (file only, default)

  Example to enable console output:

  .. code-block:: bash

      export UMPIRE_LOG_TO_CONSOLE=on

* ``UMPIRE_LOG_ASYNC``

  Enables asynchronous logging mode for improved performance in applications with
  heavy logging. In async mode, log messages are queued and written by a background
  thread, reducing overhead in performance-critical code paths.

  Valid values (case-insensitive):

  - ``1``, ``true``, ``on`` - Enable asynchronous logging
  - ``0``, ``false``, ``off`` - Use synchronous logging (default)

  Example:

  .. code-block:: bash

      export UMPIRE_LOG_ASYNC=on

* ``UMPIRE_LOG_QUEUE_SIZE``

  Sets the size of the message queue when using asynchronous logging. Larger
  queues can handle bursty logging better but use more memory. Minimum value
  is 1024. Default is 8192.

  Only used when ``UMPIRE_LOG_ASYNC`` is enabled.

  Example:

  .. code-block:: bash

      export UMPIRE_LOG_QUEUE_SIZE=16384

* ``UMPIRE_OUTPUT_DIR``

  Directory where log files will be written. Defaults to the current directory (``./``).
  If the directory does not exist, Umpire creates it; under MPI, only rank 0
  creates the directory and all ranks synchronize before opening their log
  files. When MPI is enabled but not yet initialized, Umpire cannot create the
  directory and will report an error, so ensure the directory exists (or MPI
  is initialized) before the first Umpire call.

  Example:

  .. code-block:: bash

      export UMPIRE_OUTPUT_DIR=/tmp/umpire_logs

* ``UMPIRE_OUTPUT_BASENAME``

  Base name for log files. The actual filename will be in the format:
  ``basename.pid.id.log`` where ``pid`` is the process ID and ``id`` is an
  incrementing number to ensure uniqueness. Default is ``umpire``.

  Example:

  .. code-block:: bash

      export UMPIRE_OUTPUT_BASENAME=myapp

Example Logging Configurations
-------------------------------

**Basic debugging (file only, the default):**

.. code-block:: bash

    export UMPIRE_LOG_LEVEL=INFO

**Interactive debugging (messages also echoed to stderr):**

.. code-block:: bash

    export UMPIRE_LOG_LEVEL=INFO
    export UMPIRE_LOG_TO_CONSOLE=on

**High-performance async logging:**

.. code-block:: bash

    export UMPIRE_LOG_LEVEL=DEBUG
    export UMPIRE_LOG_ASYNC=on
    export UMPIRE_LOG_QUEUE_SIZE=16384

**Custom log location:**

.. code-block:: bash

    export UMPIRE_LOG_LEVEL=INFO
    export UMPIRE_OUTPUT_DIR=/scratch/logs
    export UMPIRE_OUTPUT_BASENAME=umpire_run1
