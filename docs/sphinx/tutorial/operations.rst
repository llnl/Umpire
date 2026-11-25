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

----
Copy
----

Let's start by looking at how we copy data around. The
:class:`umpire::ResourceManager` provides an interface to copy that handles
figuring out where the source and destination pointers were allocated, and
selects the correct implementation to copy the data:

.. literalinclude:: ../../../examples/tutorial/tut_copy.cpp
   :start-after: _sphinx_tag_tut_copy_start
   :end-before: _sphinx_tag_tut_copy_end
   :language: C++

This example allocates the destination data using any valid Allocator. 


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

Setting a whole block of memory to a value (like 0) is a common operation that
most people know as a memset. Umpire provides a
:func:`umpire::ResourceManager::memset` implementation that can be applied to
any allocation, regardless of where it came from:

.. literalinclude:: ../../../examples/tutorial/tut_memset.cpp
   :start-after: _sphinx_tag_tut_memset_start
   :end-before: _sphinx_tag_tut_memset_end
   :language: C++

----------
Reallocate
----------

Reallocating CPU memory is easy, there is a function designed specifically to
do it: ``realloc``. When the original allocation was made in a different memory
however, you can be out of luck. Umpire provides a
:func:`umpire::ResourceManager::reallocate` operation:

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
