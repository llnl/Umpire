.. _uberenv:

=======
Uberenv
=======

Umpire shares its Uberenv workflow with other projects. The documentation is
therefore `shared <https://radiuss-ci.readthedocs.io/en/latest/uberenv.html#uberenv-guide>`_.

This page will provides some Umpire specific examples to illustrate the
workflow described in the documentation.

Before starting
---------------

First of all, it is worth noting that Umpire does not have dependencies, except
for CMake, which is most of the time installed externally.

That does not make the workflow useless:
Uberenv will drive Spack which will generate a host-config file with the
toolchain (including cuda if activated) and the options or variants
pre-configured.

Machine specific configuration
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

To view the different configurations for Umpire, follow the steps below:

.. code-block:: bash

  $ ls -c1 scripts/radiuss-spack-configs
  blueos_3_ppc64le_ib
  darwin
  toss_3_x86_64_ib
  blueos_3_ppc64le_ib_p9
  config.yaml

Umpire has been configured for ``toss_3_x86_64_ib`` and other systems.

Vetted specs
^^^^^^^^^^^^

To view the CI jobs for diffrent LC machines, follow the steps below:

.. code-block:: bash

  $ ls -c1 .gitlab/*jobs.yml
  .gitlab/corona-jobs.yml
  .gitlab/lassen-jobs.yml
  .gitlab/dane-jobs.yml

This shows that the CI contains jobs for corona, lassen, and dane.

.. code-block:: bash

  $ git grep -h "SPEC" .gitlab/dane-jobs.yml | grep "gcc"
      SPEC: "%gcc@4.9.3"
      SPEC: "%gcc@8.1.0"

In the example above, we see a list of the specs vetted on ``dane``/``toss_3_x86_64_ib``.

MacOS case
^^^^^^^^^^

In Umpire, the Spack configuration for MacOS contains the default compilers depending on the OS version (`compilers.yaml`), and a commented section to illustrate how to add `CMake` as an external package. You may install CMake with homebrew, for example.


Using Uberenv to generate the host-config file
----------------------------------------------

We have seen that we can safely use `gcc@8.1.0` on dane. Let us ask for the default configuration first, then produce static libs with OpenMP support and run the benchmarks:

.. code-block:: bash

  $ python scripts/uberenv/uberenv.py --spec="%gcc@8.1.0"
  $ python scripts/uberenv/uberenv.py --spec="%gcc@8.1.0~shared+openmp tests=benchmarks"

Each will generate a CMake cache file, e.g.:

.. code-block:: bash

  dane-toss_3_x86_64_ib-gcc@8.1.0-<some_hash>.cmake

Using host-config files to build Umpire
---------------------------------------

.. code-block:: bash

  $ mkdir build && cd build
  $ cmake -C <path_to>/<host-config>.cmake ..
  $ cmake --build -j .
  $ ctest --output-on-failure -T test

It is also possible to run Umpire's complete build-and-test sequence with
``rad-bat`` outside CI:

.. code-block:: bash

  $ scripts/radiuss-spack-configs/bin/rad-bat host-config \
      --host-config <path_to>/<host-config>.cmake \
      --work-dir .rad-bat-work

Using Uberenv to configure and run Leak Sanitizer
-------------------------------------------------

During development, it may be beneficial to regularly check for memory leaks. This will help avoid the possibility of having many memory leaks showing up all at once during the CI tests later on. The Leak Sanitizer can easily be configured from the root directory with:

.. code-block:: bash

  $ srun -ppdebug -N1 --exclusive python scripts/uberenv/uberenv.py --spec="%clang@9.0.0 cxxflags=-fsanitize=address"
  $ cd build
  $ cmake -C <path_to>/<host-config>.cmake ..
  $ cmake --build -j
  $ ASAN_OPTIONS=detect_leaks=1 make test

.. note::
  The host config file can be reused in order to rebuild with the same configuration if needed.

This will configure a build with Clang 9.0.0 and the Leak Sanitizer. If there is a leak in one of the tests, it can be useful to gather more information about what happened and more details about where it happened. One way to do this is to run:

.. code-block:: bash

  $ ASAN_OPTIONS=detect_leaks=1 ctest -T test --output-on-failure

Additionally, the Leak Sanitizer can be run on one specific test (in this example, the "replay" tests) with:

.. code-block:: bash

  $ ASAN_OPTIONS=detect_leaks=1 ctest -T test -R replay --output-on-failure

Depending on the output given when running the test with the Leak Sanitizer, it may be useful to use ``addr2line -e <./path_to/executable> <address_of_leak>`` to see the exact line the output is referring to.
