.. _file_allocation:

==================================
Using File System Allocator (FILE)
==================================

Umpire supports the use of file based memory allocation. When ``UMPIRE_ENABLE_FILE_RESOURCE`` 
is enabled, the environment variables ``UMPIRE_MEMORY_FILE_DIR`` can be used to determine 
where memory can be allocated from:

  ======================   ======================   =======================================================
  Variable                 Default                  Description
  ======================   ======================   =======================================================
  UMPIRE_MEMORY_FILE_DIR   ./                       Directory to create and allocate file based allocations
  ======================   ======================   =======================================================

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

^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Using Burst Buffers On Lassen
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

On Lassen, 1) download the latest version of Umpire 2) request a private node to build:

.. code-block:: bash

  $ git clone --recursive https://github.com/LLNL/Umpire.git
  $ lalloc 1 -stage storage=64

Note that ``-stage storage=64`` is needed in order to work with the Burst Buffers. 
3) Additionally, the environment variable needs to set to ``$BBPATH`` :

.. code-block:: bash

  $ export UMPIRE_MEMORY_FILE_DIR=$BBPATH/

^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Running File Resource Benchmarks
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Continue building Umpire on 1 node, and set the ``-DUMPIRE_ENABLE_FILE_RESOURCE=On`` :

.. code-block:: bash

  $ mkdir build && cd build
  $ lrun -n 1 cmake -DUMPIRE_ENABLE_FILE_RESOURCE=On -DENABLE_OPENMP=On ../ && make

To run the built-in benchmarks in Umpire from the build run:

.. code-block:: bash

  $ lrun -n 1 --threads=** ./bin/file_resource_benchmarks ##

In the snippet above, "**" is a placeholder for the amount of threads wanted to run the benchmark on. 
"##" stands for the number of array elements wanted to be passed through the benchmark. 
This number can range from 1-100,000,000,000.

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
