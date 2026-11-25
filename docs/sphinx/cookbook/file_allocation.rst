.. _file_allocation:

==================================
Using File System Allocator (FILE)
==================================

Umpire supports the use of file based memory allocation. When ``UMPIRE_ENABLE_FILE_RESOURCE``
is enabled, the environment variable ``UMPIRE_MEMORY_FILE_DIR`` can be used to determine
where memory can be allocated from:

  ======================   ======================   =======================================================
  Variable                 Default                  Description
  ======================   ======================   =======================================================
  UMPIRE_MEMORY_FILE_DIR   ./                       Directory to create and allocate file based allocations
  ======================   ======================   =======================================================

.. note::
   The value of ``UMPIRE_MEMORY_FILE_DIR`` must end with a trailing slash
   (e.g. ``/tmp/umpire/``, not ``/tmp/umpire``), as the file name is appended
   directly to it. The directory must already exist and be writable.

Each allocation is backed by a file named ``umpire_mem_<pid>_<counter>``
in that directory. The file is removed when the allocation is deallocated,
but if an application exits abnormally, stale ``umpire_mem_*`` files may be
left behind and can be deleted manually. Allocation sizes are rounded up to
a multiple of the system page size.

Requesting the allocation takes two steps: 1) getting a "FILE" allocator, 
2) requesting the amount of memory to allocate.

.. literalinclude:: ../../../examples/cookbook/recipe_filesystem_memory_allocation.cpp
   :start-after: _sphinx_tag_tut_file_allocate_start
   :end-before: _sphinx_tag_tut_file_allocate_end
   :language: C++

To deallocate:

.. literalinclude:: ../../../examples/cookbook/recipe_filesystem_memory_allocation.cpp
   :start-after: _sphinx_tag_tut_file_deallocate_start
   :end-before: _sphinx_tag_tut_file_deallocate_end
   :language: C++

The complete example is included below:

.. literalinclude:: ../../../examples/cookbook/recipe_filesystem_memory_allocation.cpp

.. note::
   This example is not built as part of the default Umpire build. To try it,
   compile it directly against an Umpire install (or build tree), e.g.:

   .. code-block:: bash

     $ c++ -std=c++20 recipe_filesystem_memory_allocation.cpp \
           -I<umpire-install>/include -L<umpire-install>/lib \
           -lumpire -lcamp -o file_example

^^^^^^^^^^^^^^^^^^^^
Building on macOS
^^^^^^^^^^^^^^^^^^^^

The FILE resource builds and runs on macOS, with two caveats:

* Apple's Clang does not ship with OpenMP support, so configure with
  ``-DENABLE_OPENMP=Off`` (OpenMP is only used to add threading to the
  file resource benchmarks; the FILE resource itself does not need it).
  Alternatively, use a compiler with OpenMP support (e.g. Homebrew GCC
  or LLVM with ``libomp``).
* Older versions of Umpire used the Linux-specific ``O_LARGEFILE`` flag
  and ``ftruncate64`` function and will not compile the FILE resource on
  macOS. If you hit errors about these identifiers, update Umpire, or
  replace them with ``0`` and ``ftruncate`` respectively (on macOS,
  ``off_t`` is already 64-bit, so no large-file variants are needed).

^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Choosing Where File Allocations Are Placed
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The performance of the FILE resource depends heavily on the file system
backing the directory pointed to by ``UMPIRE_MEMORY_FILE_DIR``. For best
results, point it at the fastest storage available on the machine, such as:

* Node-local storage (e.g. a local SSD/NVMe drive, often exposed at a
  path like ``/tmp`` or ``/l/ssd`` depending on the system)
* Burst buffers or other fast scratch storage provided by the system
* A tmpfs/ramdisk mount (e.g. ``/dev/shm``), which keeps allocations in
  memory rather than on disk

Avoid pointing ``UMPIRE_MEMORY_FILE_DIR`` at slow, shared parallel file
systems (e.g. home directories or project spaces) unless capacity matters
more than performance.

Some HPC systems require fast storage to be requested as part of the
batch job allocation (for example, a flag or directive that stages burst
buffer or node-local storage). Consult your system's documentation for
how to request such storage and for the environment variable or path
where it is mounted, then set:

.. code-block:: bash

  $ export UMPIRE_MEMORY_FILE_DIR=/path/to/fast/storage/

^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Running File Resource Benchmarks
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Build Umpire with ``-DUMPIRE_ENABLE_FILE_RESOURCE=On`` :

.. code-block:: bash

  $ mkdir build && cd build
  $ cmake -DUMPIRE_ENABLE_FILE_RESOURCE=On -DENABLE_OPENMP=On ../ && make

To run the built-in benchmarks from the build directory:

.. code-block:: bash

  $ ./bin/file_resource_benchmarks ##

In the snippet above, "##" stands for the number of array elements to be
passed through the benchmark. This number can range from 1-100,000,000,000.
On a cluster, launch the benchmark with your system's job launcher
(e.g. ``srun``, ``flux run``, ``jsrun``, or ``mpirun``) and use the
launcher's options to control the number of threads available to OpenMP.

Results should appear like:

.. code-block:: bash
  
  Array Size:   1        Memory Size: 8e-06 MB
  Total Arrays: 3               Total Memory Size: 2.4e-05 MB

  HOST
    Initialization:      0.0247461 MB/sec
    Initialization Time: 0.000969849 sec
    ---------------------------------------
    Copy:                0.890918 MB/sec
    Copy Time:           1.7959e-05 sec
    ---------------------------------------
    Scale:               0.928074 MB/sec
    Scale Time:          1.724e-05 sec
    ---------------------------------------
    Add:                 1.321 MB/sec
    Add Time:            1.8168e-05 sec
    ---------------------------------------
    Triad:               1.24102 MB/sec
    Triad Time:          1.9339e-05 sec
    ---------------------------------------
    Total Time:          0.00104323 sec

  FILE
    Initialization:      0.210659 MB/sec
    Initialization Time: 0.000113928 sec
    ---------------------------------------
    Copy:                0.84091 MB/sec
    Copy Time:           1.9027e-05 sec
    ---------------------------------------
    Scale:               0.938086 MB/sec
    Scale Time:          1.7056e-05 sec
    ---------------------------------------
    Add:                 1.28542 MB/sec
    Add Time:            1.8671e-05 sec
    ---------------------------------------
    Triad:               1.54689 MB/sec
    Triad Time:          1.5515e-05 sec
    ---------------------------------------
    Total Time:          0.000184726 sec

This benchmark run similar to the STREAM Benchmark test and can also run a benchmark 
for the additional allocators like ``UM`` for CUDA and ``DEVICE`` for HIP.
