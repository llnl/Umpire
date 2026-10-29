.. _logging_and_replay:

===================================
Logging and Replay of Umpire Events
===================================

Logging
-------
When debugging memory operation problems, it is sometimes helpful to enable
Umpire's logging facility. The logging functionality is enabled for default
builds unless ``-DUMPIRE_ENABLE_LOGGING=Off`` has been specified, in which case
it is disabled.

If Umpire logging is enabled, it may be controlled by setting the
``UMPIRE_LOG_LEVEL`` environment variable to ``Error``, ``Warning``, ``Info``,
or ``Debug``. The ``Debug`` value is the most verbose.

When ``UMPIRE_LOG_LEVEL`` has been set, events will be logged to the standard
output.

Replay
------
Umpire provides a replay capability that is focused on reproducibility. By
running an executable that uses Umpire with the environment variable
``UMPIRE_REPLAY`` set to ``On``, Umpire will emit a replay-v2 trace that can be
used later to recreate allocator construction, allocations, and deallocations
independent of the original application.

Replay-v2 records attempted calls to:

- built-in memory resource construction, serialized as ``make_allocator`` with
  ``strategy`` set to ``MemoryResource``
- :func:`umpire::ResourceManager::makeAllocator`
- :func:`umpire::Allocator::allocate`
- :func:`umpire::Allocator::deallocate`

The output is newline-delimited JSON. The first line is a header that
identifies the schema version. Each later line is a replay command. Successful
operations are recorded as a lightweight two-line lifecycle: a ``pending``
record written before execution and a ``committed`` record written after
success. If the process fails during an operation, the trace may end with a
trailing ``pending`` record.

Running with Replay
-------------------
To enable Umpire replay, one may execute as follows:

.. code-block:: bash

   UMPIRE_REPLAY="On" ./my_umpire_using_program

This will write replay output to a file with a name like
``umpire.<pid>.<uid>.stats`` in the directory specified by
``UMPIRE_OUTPUT_DIR`` (or in the current directory if it is not set).

Interpreting Results - Header
-----------------------------
The first line in the trace is a replay header. The example below is taken from
``examples/tutorial/tut_replay_log.json``:

.. literalinclude:: ../../../examples/tutorial/tut_replay_log.json
   :start-after: _sphinx_tag_doc_header_start
   :end-before: _sphinx_tag_doc_header_end
   :language: json

The header contains:

**kind**
  Always set to ``umpire_replay``

**schema**
  The replay schema version. The current format is ``v2``.

**process**
  Metadata about the process that generated the trace. This includes the OS
  process ID and the MPI rank when MPI is enabled.

**umpire_version**
  The Umpire version that produced the replay trace.

makeMemoryResource Event
------------------------
Built-in memory resources are recorded as ``make_allocator`` commands using the
``MemoryResource`` strategy:

.. literalinclude:: ../../../examples/tutorial/tut_replay_log.json
   :start-after: _sphinx_tag_doc_makememoryresource_start
   :end-before: _sphinx_tag_doc_makememoryresource_end
   :language: json

The command includes:

**seq**
  A monotonically increasing sequence number used to preserve operation order.

**allocator_id**
  A stable allocator identity within the trace.

**name**
  The allocator or resource name that will be visible through the
  :class:`umpire::Allocator`.

**strategy**
  ``MemoryResource`` for built-in memory resources.

**tracking**
  Whether the allocator was created with tracking enabled.

**args**
  Constructor arguments. For memory resources this includes the
  ``resource_name`` and serialized :class:`umpire::MemoryResourceTraits`.

**status**
  ``pending`` before execution and ``committed`` after success.

makeAllocator Event
-------------------
Each call to :func:`umpire::ResourceManager::makeAllocator` records a
``pending`` command before execution and, on success, a matching
``committed`` command. The example below shows a
:class:`umpire::strategy::QuickPool` construction:

.. literalinclude:: ../../../examples/tutorial/tut_replay_log.json
   :start-after: _sphinx_tag_doc_makeallocator_start
   :end-before: _sphinx_tag_doc_makeallocator_end
   :language: json

The ``args`` object contains the serialized constructor arguments for the
strategy. In this example, ``parent_allocator`` refers back to the previously
constructed ``HOST`` resource by allocator ID, and the pool parameters are
recorded explicitly so that replay can rebuild the same allocator.

allocate Event
--------------
Each allocation attempt records a ``pending`` command and, on success, a
matching ``committed`` command with the allocator used, a stable allocation
ID, and the requested allocation size:

.. literalinclude:: ../../../examples/tutorial/tut_replay_log.json
   :start-after: _sphinx_tag_doc_allocate_start
   :end-before: _sphinx_tag_doc_allocate_end
   :language: json

Replay uses ``allocation_id`` rather than the original process pointer value.
This keeps the trace stable across runs while still preserving allocation
lifetime.

deallocate Event
----------------
.. literalinclude:: ../../../examples/tutorial/tut_replay_log.json
   :start-after: _sphinx_tag_doc_deallocate_start
   :end-before: _sphinx_tag_doc_deallocate_end
   :language: json

The ``deallocate`` command uses the same ``pending``/``committed`` lifecycle
and references the same ``allocator_id`` and ``allocation_id`` so that the
replay runtime can free the correct allocation.

Replaying the session
---------------------
Loading the ``.stats`` file with the ``replay`` program will replay the
committed subset of this sequence of :class:`umpire::Allocator` creation,
allocations, and deallocations. Trailing ``pending`` records are preserved as
failure evidence but are not executed.

.. code-block:: bash

   ./bin/replay -i umpire.<pid>.<uid>.stats

To emit a ULTRA trace from the replayed allocator state, use:

.. code-block:: bash

   ./bin/replay -d -i umpire.<pid>.<uid>.stats

This writes ``replay<PID>.ult`` with allocator statistics sampled after each
replay command. That file can be visualized with ULTRA tooling.
