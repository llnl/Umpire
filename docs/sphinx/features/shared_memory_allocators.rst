.. _shared_memory_allocators:

========================
Shared Memory Allocators
========================

Umpire provides two different kinds of Shared Memory capabilities.
First, Umpire provides Inter-Process Communication (IPC) Shared Memory
which can be used with or without MPI. Secondly, Umpire provides
MPI3 Shared Memory which requires MPI3. Although both kinds of Shared
Memory provide a convenient way to share memory across nodes/sockets,
each type has a few unique characteristics and usage details which
will be outlined in this section of the documentation.

This :doc:`cookbook recipe <../cookbook/shared_memory_allocators>` shows how to use both the IPC and MPI3 Shared Memory allocators.

Important Notes About Shared Memory Allocators
----------------------------------------------

Because we are dealing with shared memory there are a few unique characteristics of the Shared Memory allocators
which set it apart from other Umpire allocators.

1. Once you allocate shared memory, that block of memory is fixed. If you need a bigger size, you will have to create a new one.
2. If you want to see how much memory is available for a shared memory allocator, use the ``getActualSize()`` function.
3. File descriptors are used for the shared memory. These files will be under ``/dev/shm``.
4. Although Umpire does not need to have MPI enabled in order to provide IPC Shared Memory, if users wish to associate shared memory with MPI communicators, Umpire will need to be built with MPI enabled. Of course for the MPI3 Shared Memory, MPI is required.
5. It most likely won't make sense to use memory pools with a shared memory allocator. The way shared memory allocators are implemented makes them already kind of pool-like. Since you have to give them a size when you create them, that is basically the "chunk" of memory you have to work with. Then, the shared memory allocator will manage that chunk for you. Therefore, we *do not* recommend that you use pools on top of shared memory allocators.
6. For some LC machines, running Shared Memory Allocators on the login node may produce runtime errors because the login node may not have access to the correct files. If you get an error on the login node, try a compute node instead.
7. MPI3 Shared Memory Allocators support `shared_scope` traits of `node` and `socket`. Socket scope requires Linux and MPI ranks bound such that each rank's CPU affinity mask maps to a single socket. IPC Shared Memory Allocators support `node` scope.
8. MPI3 Shared Memory Allocators do not need an explicit name during creation like IPC Shared Memory Allocators do.

Enabling Both Shared Memory Allocators
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

As of v2025.09.0, users can enable both Shared Memory allocators at the same time. Thus, we introduced a "default" shared memory
resource cmake variable, ``UMPIRE_DEFAULT_SHARED_MEMORY_RESOURCE``. The default allows a shortcut for users to simply specify 
``SHARED`` and then that default shared memory resource will be used. See table below which describes this default.

+-------------+-------------+---------+
| MPI3        | IPC         | Default |
+=============+=============+=========+
| enabled     | disabled    | MPI3    |
+-------------+-------------+---------+
| disabled    | enabled     | IPC     |
+-------------+-------------+---------+
| enabled     | enabled     | MPI3    |
+-------------+-------------+---------+
| disabled    | disabled    | N/A     |
+-------------+-------------+---------+

As indicated in the table above, if both IPC and MPI3 Shared Memory are enabled, then MPI3 is the default. (For the table above, it is
assumed that MPI is enabled.) In order to use IPC shared memory, users need to be explicit when creating the allocator. For example:

.. code-block:: cpp

   auto traits{umpire::get_default_resource_traits("SHARED::POSIX")};
   ...
   auto node_allocator{rm.makeResource("SHARED::POSIX::alloc", traits)};

Note that the ``SHARED::POSIX`` prefix is required to use IPC Shared Memory in this case. You can confirm that your allocator
is an IPC Shared Memory allocator with the following code:

.. code-block:: cpp

   if (umpire::util::matchesSharedMemoryResource("SHARED::POSIX::alloc", "POSIX")) {
     // The "SHARED::POSIX::alloc" allocator is indeed a POSIX(IPC) Shared Memory Allocator!
   }

Other Shared Memory Helper Functions
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

There are a few helper functions provided in the ``Umpire.hpp`` header that will be useful when working with 
Shared Memory allocators. For example, you can grab the MPI communicator for a particular Shared Memory allocator with:

.. code-block:: cpp

   MPI_Comm shared_allocator_comm = umpire::get_communicator_for_allocator(node_allocator, MPI_COMM_WORLD);

.. warning::
   If you use the ``umpire::get_communicators_for_allocator(...)`` helper function then you MUST
   also call ``umpire::cleanup_cached_communicators()`` function before you call ``MPI_Finalize()``
   in order to avoid memory leaks.

MPI3 socket-scoped shared memory also provides CPU-affinity preflight helpers:

.. code-block:: cpp

   std::string reason;

   // Local-rank check. This inspects only the calling rank's CPU affinity mask.
   if (!umpire::resource::affinity_maps_to_single_socket(reason)) {
     // The calling rank is not pinned to exactly one socket.
   }

   // Collective check. Every rank in the communicator must call this function.
   if (!umpire::can_use_socket_scoped_mpi3_shared_memory(MPI_COMM_WORLD, reason)) {
     // At least one rank in MPI_COMM_WORLD cannot use socket scope.
   }

Additionally, we can double check that an allocator has the ``SHARED`` memory resource by asserting:

.. code-block:: cpp

  UMPIRE_ASSERT(node_allocator.getAllocationStrategy()->getTraits().resource == umpire::MemoryResourceTraits::resource_type::shared);

Check out the cookbook for more Shared Memory examples.
