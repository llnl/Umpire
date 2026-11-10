.. _advice_device_id:

=============================================
Apply Memory Advice with a Specific Device ID
=============================================

When using unified memory on systems with CUDA GPUs, various types of memory
advice can be applied to modify how the CUDA runtime moves this memory around
between the CPU and GPU. When applying memory advice, a device ID can be used
to specify which device the advice relates to.  One type of advice that can be
applied is "preferred location", and you can specificy where you want the
preferred location of the memory to be. This can be useful for ensuring that
the memory is kept on the GPU. 

By passing a specific device id when constructing an
:class:`umpire::strategy::AllocationAdvisor`, you can ensure that the advice
will be applied with respect to that device

.. literalinclude:: ../../../examples/cookbook/recipe_advice_device_id.cpp
   :start-after: _sphinx_tag_tut_device_advice_start
   :end-before: _sphinx_tag_tut_device_advice_end
   :language: C++

The complete example is included below:

.. literalinclude:: ../../../examples/cookbook/recipe_advice_device_id.cpp
