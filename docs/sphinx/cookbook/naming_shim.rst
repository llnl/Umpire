.. _naming_shim:

==========================================
Using a Naming Shim with IPC Shared Memory
==========================================

A name is required in order to allocate memory with IPC Shared Memory Allocators.
However, using a unique name for each allocator can get tricky, especially
when dealing with many MPI tasks and in an integrated code set-up.

Thus, the :class:`umpire::strategy::NamingShim` strategy was created to make it a bit 
easier to set up and use the IPC Shared Memory Allocator.
The `NamingShim` allows you to call allocate with only 1 argument for the size in bytes.
In other words, it allows you to allocate from shared memory without providing a name.

.. note::
   Why the word "shim"? In software development, a "shim" refers to a small piece of code that bridges
   the gap between two different APIs, frameworks, or libraries. It's primary purpose is to provide
   a consistent interface or behavior while hiding the underlying differences. Since this strategy
   acts as a shim to hide the requirement of a unique name from the user, we call it the `NamingShim`.

Below is a complete example of using the `NamingShim` with the IPC Shared Memory Allocator.

.. literalinclude:: ../../../examples/cookbook/recipe_naming_shim.cpp
   :language: cpp
