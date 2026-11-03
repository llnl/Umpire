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

