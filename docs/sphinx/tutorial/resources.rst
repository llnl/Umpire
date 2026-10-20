.. _resources:

=========
Resources
=========

Each computer system will have a number of distinct places in which the system
will allow you to allocate memory. In Umpire's world, these are `memory
resources`. A memory resource can correspond to a hardware resource, but can
also be used to identify memory with a particular characteristic, like "pinned"
memory in a GPU system.

When you configure Umpire, it will create
:class:`umpire::resource::MemoryResource` s according to what is available on
the system you are building for. For each resource (defined by 
``MemoryResourceTraits::resource_type``), Umpire will create a
default :class:`umpire::Allocator` that you can use. In the previous example,
we were actually using an :class:`umpire::Allocator` created for the memory
resource corresponding to the CPU memory.

The easiest way to identify resources is by name. The "HOST" resource is always
available. We also have resources that represent global GPU memory ("DEVICE"), 
constant GPU memory ("DEVICE_CONST"), unified memory that can be accessed by 
the CPU or GPU ("UM"), host memory that can be accessed by the GPU ("PINNED"), 
and mmapped file memory ("FILE"). If an incorrect name is used or if the 
allocator was not set up correctly, the "UNKNOWN" resource name is returned.

Umpire will create an :class:`umpire::Allocator` for each of these resources,
and you can get them using the same
:func:`umpire::ResourceManager::getAllocator` call you saw in the previous
example:

.. literalinclude:: ../../../examples/tutorial/tut_resources.cpp
   :start-after: _sphinx_tag_tut_get_allocator_start
   :end-before: _sphinx_tag_tut_get_allocator_end
   :language: C++

If you would prefer to avoid exceptions when an allocator is not available,
you can instead use :func:`umpire::ResourceManager::tryGetAllocator`, which
returns a ``std::optional<umpire::Allocator>`` that is empty when the
requested allocator cannot be found:

.. code-block:: cpp

  auto& rm = umpire::ResourceManager::getInstance();
  auto maybe_allocator = rm.tryGetAllocator("HOST");

  if (maybe_allocator) {
    auto allocator = *maybe_allocator;
    // use allocator
  }

Note that since every allocator supports the same calls, no matter which resource 
it is for, this means we can run the same code for all the resources available in
the system.

While using Umpire memory resources, it may be useful to query the memory 
resource currently associated with a particular allocator. For example, if we wanted
to double check that our allocator is using the ``device`` resource, we can 
assert that ``MemoryResourceTraits::resource_type::device`` is equal 
to the return value of ``allocator.getAllocationStrategy()->getTraits().resource``. 
The test code provided in ``memory_resource_traits_tests.cpp`` shows a complete 
example of how to query this information.

.. note::
   In order to test some memory resources, you may need to configure your Umpire
   build to use a particular platform (a member of the ``umpire::Allocator``, defined by
   ``Platform.hpp``) that has access to that resource. See the `Developer's 
   Guide <../developer_guide.html>`_ for more information. 

Next, we will see an example of how to move data between resources using
operations.

.. literalinclude:: ../../../examples/tutorial/tut_resources.cpp 
