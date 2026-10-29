---
name: running-the-code
description: Information on running the Umpire code and where to find examples.
---

# Running Umpire Examples

After a build completes, the executable files will be placed in the `bin` folder within the `build` directory. You can run Umpire exectuables from the `build` directory and any corresponding output and/or output files will be placed in the `build` directory.

## Examples

Examples can be found in `Umpire/examples`. You will see that a `tutorial` and `cookbook` subdirectory exist within the `examples` directory. The `cookbook` subdirectory contains a lot of valuable how-to examples. It is called a `cookbook` because the examples within provide a "recipe" for how to do something. For example, the "recipe_no_introspection" example shows how to turn off introspection when creating a QuickPool allocator.

## Sample Run Commands For Examples

In this example, we are running the allocator.cxx example to see which allocator we "got" after running `rm.getAllocator` and to view all available allocators by name:
`./bin/allocator`

This will generate output. If there is output generated from any Umpire example or test, make sure the user can see that.

## Tests

Tests can be found in `Umpire/tests`. From the `tests` directory, you can see that we have several different kinds of tests including `unit` and `integration` (and others). We try to keep the kinds of tests consistent with the folder they are created under.

## Sample Run Commands for Tests

In this example, we are running the `strategy_tests` test:
`ctest -T test -R strategy_tests --output-on-failure`

Be sure to show the user any output, regardless of whether it is due to failure or success.
