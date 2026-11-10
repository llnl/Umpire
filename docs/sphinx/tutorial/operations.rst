.. _operations:

==========
Operations
==========

Moving and modifying data in a heterogenous memory system can be annoying. You
have to keep track of the source and destination, and often use vendor-specific
APIs to perform the modifications. In Umpire, all data modification and
movement is wrapped up in a concept we call `operations`. Full documentation
for all of these is available `here <../features/operations.html>`_. The full
code listing for each example is include at the bottom of the page.

The copy, memset, and reallocate operations are provided as free functions in
the ``umpire`` namespace, declared in ``umpire/op.hpp``:

.. code-block:: cpp

  #include "umpire/op.hpp"

When called with typed pointers (such as ``double*``), these functions take
their size argument as a number of **elements**, not bytes; when called with
``void*`` pointers, the size is a number of **bytes**.

----
Copy
----

Let's start by looking at how we copy data around. The ``umpire::copy``
function handles figuring out where the source and destination pointers were
allocated, and selects the correct implementation to copy the data. Its
arguments are ordered **source first, then destination**, followed by the
number of elements to copy (here ``size`` doubles, not
``size * sizeof(double)`` bytes):

.. literalinclude:: ../../../examples/tutorial/tut_copy.cpp
   :start-after: _sphinx_tag_tut_copy_start
   :end-before: _sphinx_tag_tut_copy_end
   :language: C++

This example allocates the destination data using any valid Allocator.

.. note::

   The deprecated ``ResourceManager::copy(dst, src, bytes)`` takes its pointer
   arguments in the opposite order (destination first) and always takes a size
   in bytes. When migrating, swap the pointers and convert the byte count to an
   element count for typed pointers.


----
Move
----

If you want to move data to a new Allocator and deallocate the old copy, Umpire
provides a :func:`umpire::ResourceManager::move` operation.

.. literalinclude:: ../../../examples/tutorial/tut_move.cpp
   :start-after: _sphinx_tag_tut_move_start
   :end-before: _sphinx_tag_tut_move_end
   :language: C++

The move operation combines an allocation, a copy, and a deallocate into one
function call, allowing you to move data without having to have the destination
data allocated. As always, this operation will work with any valid destination
Allocator.

------
Memset
------

Setting a whole block of memory to a value (like 0) is a common operation, that
most people know as a memset. Umpire provides an ``umpire::memset`` function
that can be applied to any allocation, regardless of where it came from. As
with ``std::memset``, each byte is set to the given value; the length is the
number of elements (here ``SIZE`` doubles) for typed pointers, or bytes for
``void*`` pointers:

.. literalinclude:: ../../../examples/tutorial/tut_memset.cpp
   :start-after: _sphinx_tag_tut_memset_start
   :end-before: _sphinx_tag_tut_memset_end
   :language: C++

----------
Reallocate
----------

Reallocating CPU memory is easy, there is a function designed specifically to
do it: ``realloc``. When the original allocation was made in a different memory
however, you can be out of luck. Umpire provides an ``umpire::reallocate``
operation. It takes the address of the pointer to reallocate and the new size,
which for typed pointers is a number of elements (here ``REALLOCATED_SIZE``
doubles) and for ``void*`` pointers is a number of bytes. The data is
reallocated using the allocation's existing Allocator:

.. literalinclude:: ../../../examples/tutorial/tut_reallocate.cpp
   :start-after: _sphinx_tag_tut_realloc_start
   :end-before: _sphinx_tag_tut_realloc_end
   :language: C++

This method returns a pointer to the reallocated data. Like all operations,
this can be used regardless of the Allocator used for the source data.

--------
Listings
--------

Copy Example Listing

.. literalinclude:: ../../../examples/tutorial/tut_copy.cpp

Move Example Listing

.. literalinclude:: ../../../examples/tutorial/tut_move.cpp

Memset Example Listing

.. literalinclude:: ../../../examples/tutorial/tut_memset.cpp

Reallocate Example Listing

.. literalinclude:: ../../../examples/tutorial/tut_reallocate.cpp
