.. _file_output:

========
File I/O
========

Umpire provides support for writing files containing log and replay data,
rather than directing this output to stdout. When logging or replay are
enabled, the following environment variables can be used to determine where the
output is written:


      ===========================  ======== ===============================================================================
      Variable                     Default  Description
      ===========================  ======== ===============================================================================
      ``UMPIRE_OUTPUT_DIR``        .        Directory to write log and replay files
      ``UMPIRE_OUTPUT_BASENAME``   umpire   Basename of logging and replay files


The values of these variables are used to construct unique filenames for
output. The extension ``.log`` is used for logging output, and ``.stats`` for
replay output. The filenames additionally contain the process ID and a unique
integer that is used to make multiple files with the same basename. This
ensures that multiple runs with the same IO configuration do not overwrite
files.

Replay ``.stats`` files use the replay-v2 JSONL format: one header record
followed by ``make_allocator``, ``allocate``, and ``deallocate`` command
records, each with ``pending`` and ``committed`` lifecycle states when the
operation succeeds.

The format of the filenames is:

.. code-block:: bash

    <UMPIRE_OUTPUT_BASENAME>.<PID>.<UID>.<log|error|stats>

If Umpire is compiled without MPI support, then rank will always be 0.
