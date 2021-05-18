|license| |codecov| |build|

.. |license| image:: https://img.shields.io/badge/license-MIT-green.svg
    :target: https://github.com/bioinfologics/bsg/blob/master/LICENSE
.. |codecov| image:: https://codecov.io/gh/bioinfologics/sdg/branch/master/graph/badge.svg
    :target: https://codecov.io/gh/bioinfologics/sdg
.. |build| image:: https://travis-ci.org/bioinfologics/sdg.svg?branch=master
    :target: https://travis-ci.org/bioinfologics/sdg

Sequence Distance Graph
========================

The Sequence Distance Graph (**SDG**) is a framework to work with genome graphs and sequencing data. It provides a workspace built around a Sequence Distance Graph, datastores for paired, linked and long reads, read mappers, and k-mer counters. It can be used to perform different types of sequence analyses.

SDG can be run on Linux and MacOS, and requires enough RAM to hold the WorkSpace completely in memory, which will depend on the dataset. Space to hold the uncompressed sequences on the datastores on disk will also be required.

SDG can be used as a Python module through its pybind interface. Older versions used a SWIG interface.

For examples on how to use SDG please visit https://bioinfologics.github.io/sdg_examples/ (please note the examples are based on the release version).

SDG has been published on an F1000 software article: https://f1000research.com/articles/8-1490/v1

Installing SDG releases
#######################

SDG can be installed via pre-compiled binaries from https://github.com/bioinfologics/sdg/releases. The binaries have been built using Python3 and GCC version 6 from the Ubuntu package manager for the Linux version. The MacOS version dependencies were obtained using Homebrew (Python3, GCC-6 and SWIG).

Compiling the release versions requires the following dependencies:

- GCC 6+
- CMake 3.14.3+
- SWIG 4.0.0 (For Python interface)
- Python 3.7+ (For Python interface)
- Git LFS (For tests)

The installation process consists of generating the configuration files using CMake and using make. By default make install will install to system wide directories, to specify a different directory use `CMAKE_INSTALL_PREFIX <https://cmake.org/cmake/help/v3.13/variable/CMAKE_INSTALL_PREFIX.html#cmake-install-prefix>`_.

.. code-block:: bash

    wget https://github.com/bioinfologics/sdg/archive/v1.0_rc8.tar.gz
    cd sdg-1.0_rc8
    mkdir build
    cd build
    cmake ../
    make
    make test
    make install


Compiling SDG's master branch
#######################

The master branch is undergoing migration to a new pybind interface and further support for more analyses. Use this at your own risk.

.. code-block:: bash

    git clone https://github.com/bioinfologics/sdg
    cd sdg
    mkdir build
    cd build
    cmake ../
    make
    make install


Usage
#####

Working with SDG typically involves two different stages: creating a *WorkSpace* with the data and mappings, and analysing this *WorkSpace*. SDG includes command line tools to create *DataStores*, *KmerCounts*, and *WorkSpaces*, and map reads within a *WorkSpace*.

Command line tools
########################

sdg-datastore
*************************

Creates a *Datastore* from raw reads and can process paired, 10x or long reads. An output prefix is specified as a parameter and a <prefix>.prseq, <prefix>.lrseq or <prefix>.loseq file is generated.

sdg-kmercounter
*************************

Creates a *KmerCounter* indexing a graph from a *WorkSpace* or GFA, or works with an already generated one. A count can be added directly from raw reads or from a datastore. The *KmerCounter* is persisted on disk to a file with extension 'sdgkc'.

sdg-workspace
*************************

Creates a *WorkSpace* from a base graph or works with an already generated one. *Datastores* and *KmerCounters* can be added. The *WorkSpace* is persisted on disk to a file with extension 'sdgws'.

sdg-dbg
*************************

Creates a *WorkSpace* from a *PairedReadDatastore* or FASTQ files by building a deBruijn graph and using this as the base graph. Counts for the k-mers from the graph and raw reads are added to the workspace.

sdg-mapper
*************************

Maps reads within a *WorkSpace*. An updated *WorkSpace* is produced and dumped to the specified prefix.

Citing SDG
#######
Yanes L, Garcia Accinelli G, Wright J et al. A Sequence Distance Graph framework for genome assembly and analysis [version 1; peer review: 2 approved]. F1000Research 2019, 8:1490
(https://doi.org/10.12688/f1000research.20233.1)
