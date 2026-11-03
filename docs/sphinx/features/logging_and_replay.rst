.. _logging_and_replay:

===================================
Logging and Replay of Umpire Events            
===================================

Logging
-------
When debugging memory operation problems, it is sometimes helpful to enable
Umpire's logging facility.  The logging functionality is enabled for default
builds unless ``-DUMPIRE_ENABLE_LOGGING=Off`` has been specified in which case it is
disabled.

If Umpire logging is enabled, it may be controlled by setting the
``UMPIRE_LOG_LEVEL`` environment variable to ``Error``, ``Warning``, ``Info``,
or ``Debug``.  The ``Debug`` value is the most verbose.

Read more about advanced logging configurations :doc:`here <logging_configuration>`.

When ``UMPIRE_LOG_LEVEL`` has been set, events will be logged to the standard
output.

Replay
-------
Umpire provides a lightweight replay capability that can be used to investigate
performance of particular allocation patterns and reproduce bugs. By running an
executable that uses Umpire with the environment variable ``UMPIRE_REPLAY`` set
to ``On``, Umpire will emit information for the following Umpire events:

- **version**
  :func:`umpire::get_major_version`,
  :func:`umpire::get_minor_version`,
  and :func:`umpire::get_patch_version`
- **makeMemoryResource**
  :func:`umpire::resource::MemoryResourceRegistry::makeMemoryResource`
- **makeAllocator** :func:`umpire::ResourceManager::makeAllocator`
- **allocate** :func:`umpire::Allocator::allocate`
- **deallocate** :func:`umpire::Allocator::deallocate`

Running with Replay
-------------------
To enable Umpire replay, one may execute as follows:

.. code-block:: bash

   UMPIRE_REPLAY="On" ./my_umpire_using_program

will write Umpire replay events to a file with a name like
``umpire.<pid>.<uid>.stats`` in the directory specified by
``UMPIRE_OUTPUT_DIR`` (or in the current directory if it is not set). The
resulting ``.stats`` file contains JSON-formatted lines with the following
kinds of information:

Interpretting Results - Version Event
-------------------------------------
The first event captured is the **version** event which shows the version
information. The example below is taken from the file
``examples/tutorial/tut_replay_log.json``, which contains the contents of a
replay ``.stats`` file in JSON format:

.. literalinclude:: ../../../examples/tutorial/tut_replay_log.json
   :start-after: _sphinx_tag_doc_version_start
   :end-before: _sphinx_tag_doc_version_end
   :language: json

Each line contains the following set of common elements:

**kind**
  Always set to ``replay``

**uid**
  This is the MPI rank of the process generating the event for
  mpi programs or the PID for non-mpi.

**timestamp**
  Set to the time when the event occurred.

**event**
  Set to one of: ``version``, ``makeMemoryResource``,
  ``makeAllocator``, ``allocate``, or ``deallocate``

**payload**
  Optional and varies upon event type

**result**
  Optional and varies upon event type

As can be seen, the *major*, *minor*, and *patch* version numbers are captured
within the *payload* for this event.

makeMemoryResource Event
------------------------
Next you will see events for the creation of the default memory resources
provided by Umpire with the **makeMemoryResource** event:

.. literalinclude:: ../../../examples/tutorial/tut_replay_log.json
   :start-after: _sphinx_tag_doc_makememoryresource_start
   :end-before: _sphinx_tag_doc_makememoryresource_end
   :language: json

The *payload* shows that a memory resource was created for *HOST*, *DEVICE*,
*PINNED*, *UM*, and *DEVICE_CONST* respectively. Note that this could also
be done with the *FILE* memory resource. The *result* is a reference
to the object that was created within Umpire for that resource.

makeAllocator Event
-------------------
The **makeAllocator** event occurs whenever a new allocator instance is being
created.  Each call to **makeAllocator** will generate a pair of JSON lines.  The
first line will show the intent of the call and the second line will show both
the intent and the result.  This is because the makeAllocator call can fail
and keeping both the intent and result allows us to reproduce this failure
later.

:class:`umpire::Allocator`:

.. literalinclude:: ../../../examples/tutorial/tut_replay_log.json
   :start-after: _sphinx_tag_doc_makeallocator_start
   :end-before: _sphinx_tag_doc_makeallocator_end
   :language: json

The *payload* shows how the allocator was constructed.  The *result* shows the
reference to the allocated object.

allocate Event
--------------
Like the **makeAllocator** event, the **allocate** event is captured as an
intention/result pair so that an error may be replayed in the event that
there is an allocation failure.

.. literalinclude:: ../../../examples/tutorial/tut_replay_log.json
   :start-after: _sphinx_tag_doc_allocate_start
   :end-before: _sphinx_tag_doc_allocate_end
   :language: json

The *payload* shows the object reference of the allocator and the size of the
allocation request.  The *result* shows the pointer to the memory allocated.

deallocate Event
----------------
.. literalinclude:: ../../../examples/tutorial/tut_replay_log.json
   :start-after: _sphinx_tag_doc_deallocate_start
   :end-before: _sphinx_tag_doc_deallocate_end
   :language: json

The *payload* shows the reference to the allocator object and the pointer
to the allocated memory that is to be freed.

Replaying the session
---------------------
Loading the ``.stats`` file with the ``replay`` program will replay this
sequence of :class:`umpire::Allocator` creation, allocations, and
deallocations:

.. code-block:: bash

   ./bin/replay -i umpire.<pid>.<uid>.stats
