.. _device_ipc:

===========================
Sharing GPU Memory with IPC
===========================

Umpire provides a ``DeviceIpcAllocator`` strategy that allows GPU memory to be
shared between processes on the same node using CUDA or HIP IPC handles. This
strategy uses the shared memory infrastructure in Umpire to coordinate the
sharing of GPU memory between processes.

How It Works
------------

The ``DeviceIpcAllocator`` works by:

1. Having one process (the "leader", determined by the "scope"-local MPI rank 0) physically allocate GPU memory
2. The leader process gets an IPC handle for that memory and stores it in CPU shared memory
3. Other processes retrieve the IPC handle from CPU shared memory and import the GPU memory
4. MPI barriers are used to synchronize between processes and ensure safe access

This allows multiple processes to share GPU memory efficiently, which is useful
for multi-process applications that need to operate on the same data.

Using DeviceIpcAllocator
------------------------

A ``DeviceIpcAllocator`` can be created using the Umpire ResourceMangager. It
doesn't require any additional arguments apart from a name. By default, device
memory will be allocated using the "DEVICE" resource. Optionally, you can
provide a device allocator, as well as a "scope" argument that determines who
will share each GPU allocation. 

Here's an example:

.. literalinclude:: ../../../examples/cookbook/recipe_device_ipc.cpp
   :language: cpp

The scope argument can either be "socket" or "node". The "socket" scope means
that all processes using the same socket (as identified by the PCI address of
the GPU) will share the GPU memory, while the "node" scope means that all
processes on the same node (as determined by MPI_COMM_TYPE_SHARED) will share
the GPU memory.

Loading Data from a File into Shared GPU Memory
-----------------------------------------------

A common use case is reading a large data set from disk once per socket and
sharing it between all ranks on that socket, rather than replicating it in
every rank's GPU memory. The following recipe reads a tetrahedral mesh (three
arrays of doubles for the node coordinates and four arrays of integers for the
connectivity) from a file into socket-scoped shared GPU memory.

The key points are:

1. Every rank reads (or receives via broadcast) the array sizes, since all
   ranks in a scope must request identically-sized allocations in the same
   order.
2. ``umpire::get_communicator_for_allocator`` is used to retrieve the scope
   (socket) communicator, and only the scope-leader rank (rank 0 in that
   communicator) reads the file contents and copies them into the GPU arrays.
3. A barrier on the scope communicator ensures the data is in place before
   any rank uses the shared memory.

Note that while the application-level coordination (broadcasting sizes, file
I/O) can use any MPI communicator, the ``DeviceIpcAllocator`` constructor,
``allocate``, and ``deallocate`` calls are collective and internally split
MPI_COMM_WORLD to form the socket scope, so all ranks in MPI_COMM_WORLD must
participate in those calls.

.. literalinclude:: ../../../examples/cookbook/recipe_device_ipc_from_file.cpp
   :language: cpp

Limitations
-----------

- Requires UMPIRE_ENABLE_MPI to be enabled
- Requires UMPIRE_ENABLE_IPC_SHARED_MEMORY to be enabled
