.. _hpctoolkit:

==========
HPCToolKit
==========

This page will describes the process and series of steps to analyze Umpire specific 
applications with `HPCToolKit <https://github.com/HPCToolkit/hpctoolkit>`_.

Using HPCToolKit
----------------

LLNL's documentation for using HPCToolKit for general analysis is a great
starting resource and can be found `here <https://hpc.llnl.gov/training/tutorials/livermore-computing-resources-and-environment#performance-analysis>`_. The HPCToolKit manual can be found `here <http://hpctoolkit.org/manual/HPCToolkit-users-manual.pdf>`_.

The LC machines have ``hpctoolkit`` installed as a module which can be 
loaded with ``module load hpctoolkit``. The rest of this page will describe 
the steps for specific analysis examples with Umpire.

Getting Started
^^^^^^^^^^^^^^^

Below is the basic (Umpire-specific) set up to load, build with, and run with HPCToolKit:

.. code-block:: bash

  $ ssh lassen
  $ module load hpctoolkit
  $ cmake -DENABLE_CUDA=On -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-O3 -g" -DCMAKE_C_FLAGS="-O3 -g"
  $ make -j
  $ hpcrun -e CPUTIME ./bin/executable
  $ hpcstruct ./bin/executable
  $ hpcprof -S executable.hpcstruct hpctoolkit-executable-measurements-<job_id>/

.. note::
  The HPCToolKit manual recommends building with a fully optimized version
  of the executable (hence, the added flags in the ``cmake`` command).

.. note::
   The ``hpcrun`` command can measure certain "events". Those events are added with a ``-e``
   argument and include things like ``CPUTIME``, ``gpu=nvidia``, ``gpu=nvidia,pc``, ``IO``,
   ``MEMLEAK``, etc. A full list of the possible events can be found by running ``hpcrun -L``.

After running the ``hpcrun`` command, a ``hpctoolkit-executable-measurements-<job_id>/`` folder
will be generated. After running the ``hpcstruct`` command, a ``executable.hpcstruct`` file will
be generated. These two generated items will then become input used with ``hpcprof``, as shown
above. If the file that you are analyzing is large or is using a lot of resources, then 
``hpcprof-mpi`` could be better to use. The ``hpcprof-mpi`` command looks the same otherwise. The
result of either ``hpcprof`` command is a generated "database" folder.

At this point, we need the HPCViewer program to view the resulting database. The easiest way
to engage with the HPCViewer is to do it locally. Therefore, we can tar up the generated 
database folder and use ``scp`` to send it to a local machine. For example:

.. code-block:: bash

   $ tar -czvf database.tar hpctoolkit-executable-database-<job_id>/
   $ scp username@lassen.llnl.gov:path/to/database.tar .
   $ tar -xzvf database.tar

From here, you can open the HPCViewer and select the untarred database folder we just sent over
to be viewed. More information on how to use HPCViewer will be provided in the next section.

Otherwise, using HPCViewer from the command line can be tricky since we need X11 forwarding. 
In order to have X11 forwarding available, we have to ssh into the LC machine and compute node 
a little differently:

.. code-block:: bash

   $ ssh -YC lassen
   $ bsub -XF -W 60 -nnodes 1 -Is /bin/bash

.. note::
   If that doesn't work, you can also try ssh'ing into the LC machine with ``ssh -X -S none lassen``.

From here we can run the same steps as before (listed at the top of this section). When we
have generated the database folder, we will just call the hpcviewer program with 
``hpcviewer hpctoolkit-executable-database-<job_id>/``. LLNL's documentation for HPCToolKit
also provides an example command to use the ``hpctraceviewer`` tool.

Using HPCViewer
^^^^^^^^^^^^^^^

Once you have your own version of HPCViewer locally, it is very easy to launch and open up the 
database folder generated earlier. You can do this with just ``./hpcviewer`` and selecting the
right database folder.

For our use cases, we mostly used the "Hot Child" feature, but that is by no means the most
valuable or most important feature that HPCViewer offers. To learn more about what HPCViewer
can do, the instruction manual is `here <http://hpctoolkit.org/download/hpcviewer/>`_.

.. note::
   Depending on what's available on your local machine, you may have to download or update Java
   in order to run hpcviewer. There are instructions `here <http://hpctoolkit.org/download/hpcviewer/>`_
   for hpcviewer. You can get Java 8 from `here <https://www.oracle.com/java/technologies/javase/javase-jdk8-downloads.html>`_.


Running with Hatchet
^^^^^^^^^^^^^^^^^^^^

`Hatchet <https://github.com/hatchet/hatchet>`_ is a tool that can better analyze performance metrics given from a variety of tools,
including HPCToolKit. Using Hatchet to analyze the output from HPCToolKit can help visualize
the performance of different parts of the same program.

To use Hatchet, we create a HPCToolKit analysis, just as before, but this time there is a
specialized ``hpcprof-mpi`` command needed when generating the database folder. Below is an
example:

.. code-block:: bash

  $ module load hpctoolkit
  $ cmake -DENABLE_CUDA=On -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-O3 -g" -DCMAKE_C_FLAGS="-O3 -g"
  $ make -j
  $ hpcrun -e CPUTIME ./bin/executable
  $ hpcstruct ./bin/executable
  $ hpcprof-mpi --metric-db yes -S executable.hpcstruct hpctoolkit-executable-measurements-<job_id>/

The flag, ``--metric-db yes``, is an optional argument to ``hpcprof-mpi`` that allows `Hatchet
<https://hatchet.readthedocs.io/en/latest/index.html>`_ to better interpret information given
from HPCToolKit. Without it, it will be very hard to get Hatchet to understand the HPCToolKit
output.

We've now generated a HPCToolKit database folder which Hatchet can read. Now we need to launch
Hatchet and get started with some analysis. Below is a Python3 interpreter mode example:

.. code-block:: bash

   $ python3 #start the python interpreter
   $ import hatchet as ht #import hatchet
   $ dirname = "hpctoolkit-executable-database-<job_id>" #set var to hpctoolkit database
   $ gf = ht.GraphFrame.from_hpctoolkit(dirname) #set up the graphframe for hatchet that uses database

   $ print(gf.tree(depth=3)) #This is to check briefly that I recognize my tree by checking the root node + a couple sub-nodes
   $ print(len(gf.graph)) #I can also verify the tree by checking the length of the graphframe
   $ print(gf.dataframe.shape) #I can also print out the 'shape' of the tree (depth x column_metrics)
   $ print(list(gf.dataframe.columns)) #I can print out all the column_metrics (e.g. "time", "nid", etc.)
   $ print(gf.dataframe.index.names) #I can also print the node names (may be kind of confusing unless you know what you're looking for)

   $ query1 = [{"name": "119:same_order\(umpire::Allocator\)"}, "*"] #Set up a query method to filter for the "same_order" sub tree
   $ filtered_gf = gf.filter(query1) #apply the query method as a filter on the original tree
   $ print(len(filtered_gf.graph)) #verifying that I now have a subtree (length will be smaller)
   $ print(filtered_gf.tree(metric_column="time (inc)")) #printing the new filtered subtree by inclusive time metric
   $ print(filtered_gf.tree()) #printing the whole filtered tree as is
 
   $ query2 = [{"name": "120:reverse_order\(umpire::Allocator\)"}, "*"] #Set up a query method to filter for the "reverse_order" sub tree
   $ filtered_gf_rev = gf.filter(query2) #apply the query method as a filter on the original tree
   $ print(len(filtered_gf_rev.graph)) #verifying that I now have a subtree (length will be smaller)
   $ print(filtered_gf_rev.tree(metric_column = "time (inc)")) #printing the new filtered subtree by inclusive time metric
 
   $ filtered_gf.drop_index_levels() #As-is, the tree will include info for ranks - if that isn't needed, this function drops that info
   $ filtered_gf.dataframe #this provides a spreadsheet of the data that is populating the graphframe (what the tree shows)
   $ filtered_gf.dataframe.iloc[0] #gives the first entry of the spreadsheet, here that is the root node of the filtered tree
   $ filtered_gf.dataframe.iloc[0,0] #gives the first part of the first entry of the spreadsheet (here, it's the inclusive time) 

   $ gf3 = filtered_gf - filtered_gf_rev #Stores the diff between two (comparable) trees in gf3
   $ print(gf3.tree()) #prints the diff tree
   $ gf3.dataframe #outputs the spreadsheet of data that populates the diff tree


This example was set up to analyze the performance of the ``no-op_stress_test.cpp`` benchmark file
from the Umpire repo. It compares the performance from one part of the program (i.e., the part that 
measures the performance when doing deallocations in the "same order" as they were allocated) versus
another part of the same program (i.e., the part that measures the performance when doing deallocations
in the "reverse order" as they were allocated). 

In Hatchet, these two parts show up as subtrees within the entire call path tree of my example program.
Therefore, I can compare one subtree to another in terms of performance (in my case, I compared in terms
of inclusive time).

Analyzing results
------------------

After opening up a database folder in HPCViewer or analyzing the call paths in Hatchet, we can compare the performance
(or whatever measurement we are looking at) of different parts of the program against other parts and try to find 
performance trends. In Umpire's use case, we plan to use Hatchet as part of our CI to find out if integrating a new
commit into our repository increases the performance by a certain threshold or more. If so, our CI test will fail. Our
process looks something like:

* Grab the example program's database from the develop branch
* Grab the example program's database from a different branch I want to compare against
* Create a graphframe for each database
* Create a filtered graphframe for each that focuses on the specific part of the program I want to measure against
* Compare the inclusive time for each filtered graphframe (or whatever metric I want to analyze)
* If the metric (e.g., inclusive time) of the new branch's filtered graphframe is more than ``threshold`` more than that of develop's, then fail the test!
