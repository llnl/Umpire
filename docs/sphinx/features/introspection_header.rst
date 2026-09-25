.. _introspection_header:

==========================
Header-Based Introspection
==========================
By default, Umpire tracks every allocation in a global allocation map so that
introspection queries like :func:`umpire::Allocator::getSize` can be answered
for any pointer. Maintaining this map adds overhead to every allocate and
deallocate call, which can be significant for cheap HOST allocations.

Building Umpire with header-based introspection replaces the allocation map
with a small header stored directly in front of each allocation. Each
allocation is padded by ``umpire::util::allocation_header_size`` bytes (one
``alignof(std::max_align_t)``-aligned block, 32 bytes on typical 64-bit
platforms), and the header holding the size and owning allocator is written at
the start of the padded block. Introspection queries then read the header in
constant time, with no locks and no global data structure.

Build Configuration
-------------------
Header-based introspection is enabled with:

- ``cmake ... -DUMPIRE_ENABLE_INTROSPECTION_HEADER=On ...``

This is a compile-time option: when it is enabled, the allocation map is not
used, and the two mechanisms can never be active at the same time.

.. warning::
    Header-based introspection requires that all memory provided by Umpire
    can be read from the host, since headers are written and read directly.
    Builds using memory resources that are not host-accessible (for example
    device memory) are not supported with this option. It is also
    incompatible with ``UMPIRE_ENABLE_BACKTRACE``, which stores per-allocation
    backtraces in the allocation map.

Supported Queries
-----------------
The following work as usual, for base pointers returned by
:func:`umpire::Allocator::allocate`:

- :func:`umpire::Allocator::getSize` and :func:`umpire::ResourceManager::getSize`
- :func:`umpire::ResourceManager::getAllocator` and ``hasAllocator`` for a pointer
- :func:`umpire::ResourceManager::findAllocationRecord`
- Operations on base pointers: ``copy``, ``memset``, ``reallocate``, ``move``
- The per-allocator counters :func:`umpire::Allocator::getCurrentSize`,
  ``getHighWatermark``, ``getActualSize``, and ``getAllocationCount``, which
  are maintained independently of the allocation map
- The error when deallocating a pointer with the wrong Allocator
- Zero-byte allocations

Unsupported Queries
-------------------
The following require enumerating or extending the allocation map and are not
available; they either throw an ``umpire::runtime_error`` or return empty
results:

- Looking up a pointer offset into an allocation (only base pointers can be
  found from a header)
- :func:`umpire::get_allocator_records` and
  :func:`umpire::print_allocator_records` (including the per-allocation leak
  report at shutdown; the total leaked bytes are still reported)
- ``umpire::pointer_overlaps`` and ``umpire::pointer_contains``
- Registering external allocations with
  :func:`umpire::ResourceManager::registerAllocation`
- Allocation names passed to ``allocate(name, size)`` are not retained

.. note::
    Because the user pointer is shifted past the header,
    :class:`umpire::strategy::AlignedAllocator` cannot guarantee alignments
    larger than ``alignof(std::max_align_t)`` in this mode, and
    :class:`umpire::strategy::FixedPool` object sizes must account for
    ``umpire::util::allocation_header_size`` bytes of padding per object.

Example
-------
The introspection calls available in this mode are shown below:

.. literalinclude:: ../../../examples/introspection_header.cpp
   :start-after: _sphinx_tag_tut_header_alloc_start
   :end-before: _sphinx_tag_tut_header_alloc_end
   :language: C++

.. literalinclude:: ../../../examples/introspection_header.cpp
   :start-after: _sphinx_tag_tut_header_query_start
   :end-before: _sphinx_tag_tut_header_query_end
   :language: C++

.. literalinclude:: ../../../examples/introspection_header.cpp
   :start-after: _sphinx_tag_tut_header_stats_start
   :end-before: _sphinx_tag_tut_header_stats_end
   :language: C++

The complete example is included below:

.. literalinclude:: ../../../examples/introspection_header.cpp

Performance
-----------
The ``introspection_header_benchmarks`` benchmark (built when
``UMPIRE_ENABLE_BENCHMARKS`` is on) compares the allocation map and the
allocation header mechanisms directly in one binary, and measures the
end-to-end allocate/getSize/deallocate path with whichever mechanism the
build was configured to use. Comparing its output between a default build
and a ``UMPIRE_ENABLE_INTROSPECTION_HEADER=On`` build shows the overhead
saved on the allocation path.

If introspection is not needed at all for an Allocator, it can also be
disabled per-allocator at runtime; see :doc:`../cookbook/no_introspection`.
