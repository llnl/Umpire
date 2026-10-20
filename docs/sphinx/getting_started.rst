.. _getting_started:

===============
Getting Started
===============

This page provides information on how to quickly get up and running with Umpire.

------------
Installation
------------

Umpire is hosted on GitHub `here <https://github.com/LLNL/Umpire>`_.
To clone the repo into your local working space, type:

.. code-block:: bash

  $ git clone --recursive https://github.com/LLNL/Umpire.git


The ``--recursive`` argument is required to ensure that the *BLT* submodule is
also checked out. `BLT <https://github.com/LLNL/BLT>`_ is the build system we
use for Umpire.


^^^^^^^^^^^^^^^
Building Umpire
^^^^^^^^^^^^^^^

Umpire uses CMake and BLT to handle builds. Make sure that you have a modern
compiler loaded and the configuration is as simple as:

.. code-block:: bash

  $ mkdir build && cd build
  $ cmake ../

By default, Umpire will only support host memory. Additional backends for
device support can be enabled using the options detailed in
:doc:`advanced_configuration`. CMake will provide output about which compiler
is being used and the values of other options. Once CMake has completed, Umpire
can be built with Make:

.. code-block:: bash

  $ make

For more advanced configuration options, see :doc:`advanced_configuration`.

^^^^^^^^^^^^^^^^^
Installing Umpire
^^^^^^^^^^^^^^^^^

To install Umpire, just run:

.. code-block:: bash

  $ make install

Umpire install files to the ``lib``, ``include`` and ``bin`` directories of the
``CMAKE_INSTALL_PREFIX``. Additionally, Umpire installs a CMake configuration
file that can help you use Umpire in other projects. By setting `umpire_DIR` to
point to the root of your Umpire installation, you can call
``find_package(umpire)`` inside your CMake project and Umpire will be
automatically detected and available for use.

When using Umpire in your CMake project, link against the namespaced target
``umpire::umpire``:

.. code-block:: cmake

  find_package(umpire REQUIRED)
  target_link_libraries(your_target umpire::umpire)

For backwards compatibility, the non-namespaced target ``umpire`` is still
available but deprecated. Using namespaced targets provides better error
diagnostics when ``find_package()`` is missing.

-----------
Basic Usage
-----------

Let's take a quick tour through Umpire's most important features. A complete
listing you can compile is included at the bottom of the page. First, let's
grab an Allocator and allocate some memory. This is the interface through which
you will want to access data:

.. code-block:: cpp

  auto& rm = umpire::ResourceManager::getInstance();
  umpire::Allocator allocator = rm.getAllocator("HOST");

  float* my_data = static_cast<float*>(allocator.allocate(100*sizeof(float)));


This code grabs the default allocator for the host memory, and uses it to
allocate an array of 100 floats. We can ask for different Allocators to
allocate memory in different places. Let's ask for a device allocator:

.. code-block:: cpp

  umpire::Allocator device_allocator = rm.getAllocator("DEVICE");

  float* my_data_device = static_cast<float*>(device_allocator.allocate(100*sizeof(float));

This code gets the default device allocator, and uses it to allocate an array
of 100 floats. Remember, since this is a device pointer, there is no guarantee
you will be able to access it on the host.  Luckily, Umpire's ResourceManager
can copy one pointer to another transparently. Let's copy the data from our
first pointer to the DEVICE-allocated pointer.

.. code-block:: cpp

  rm.copy(my_data, my_data_device);

To free any memory allocated, you can use the deallocate function of the
Allocator, or the ResourceManager. Asking the ResourceManager to deallocate
memory is slower, but useful if you don't know how or where an allocation was
made:

.. code-block:: cpp

  allocator.deallocate(my_data); // deallocate using Allocator
  rm.deallocate(my_data_device); // deallocate using ResourceManager
