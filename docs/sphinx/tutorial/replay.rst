.. _replay:

======
Replay
======
Umpire provides a replay capability for reproducing allocator behavior and
debugging Umpire issues independent of the original application.

Input Example
-------------
When replay is enabled, Umpire writes replay-v2 JSONL into a ``.stats`` file.
The first line is a header and each later line is an operation record with a
``pending`` or ``committed`` lifecycle status. This file can be used as input
to the ``replay`` application (available under the ``bin`` directory), which
reconstructs committed allocator creation, allocation, and deallocation
activity from the recorded trace.

The file ``tut_replay.cpp`` makes a :class:`umpire::strategy::QuickPool`:

.. literalinclude:: ../../../examples/tutorial/tut_replay.cpp
   :start-after: _sphinx_tag_tut_replay_make_allocate_start
   :end-before: _sphinx_tag_tut_replay_make_allocate_end
   :language: C++

This allocator is used to perform some randomly sized allocations, and later
free them:

.. literalinclude:: ../../../examples/tutorial/tut_replay.cpp
   :start-after: _sphinx_tag_tut_replay_allocate_start
   :end-before: _sphinx_tag_tut_replay_allocate_end
   :language: C++

.. literalinclude:: ../../../examples/tutorial/tut_replay.cpp
   :start-after: _sphinx_tag_tut_replay_dealocate_start
   :end-before: _sphinx_tag_tut_replay_dealocate_end
   :language: C++

Running the Example
-------------------
Running this program:

.. code-block:: bash

   UMPIRE_REPLAY="On" ./bin/examples/tutorial/tut_replay

will write replay output to a file with a name like
``umpire.<pid>.<uid>.stats`` in the current directory (or in the directory
specified by ``UMPIRE_OUTPUT_DIR``). A minimal example looks like:

.. literalinclude:: ../../../examples/tutorial/tut_replay_log.json
   :start-after: _sphinx_tag_doc_header_start
   :end-before: _sphinx_tag_doc_deallocate_end
   :language: json

Replaying the session
---------------------
Loading this ``.stats`` file with the ``replay`` program will replay this
committed sequence of :class:`umpire::Allocator` creation, allocations, and
deallocations:

.. code-block:: bash

   ./bin/replay -i umpire.<pid>.<uid>.stats

To also generate a ULTRA trace from the replayed allocator state:

.. code-block:: bash

   ./bin/replay -d -i umpire.<pid>.<uid>.stats

This writes ``replay<PID>.ult`` in the working directory.
