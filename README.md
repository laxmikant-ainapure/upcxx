### UPC\+\+: a PGAS extension for C\+\+ ###

UPC++ is a parallel programming extension for developing C++ applications with the partitioned
global address space (PGAS) model.  UPC++ has three main objectives:

* Provide an object-oriented PGAS programming model in the context of the popular C++ language

* Expose useful asynchronous parallel programming idioms unavailable in traditional SPMD models, such as
  remote function invocation and continuation-based operation completion, to support complex scientific
  applications
 
* Offer an easy on-ramp to PGAS programming through interoperability with other existing parallel
  programming systems (e.g., MPI, OpenMP, CUDA)

For a description of how to use UPC++, please refer to the [programmer's guide](docs/guide/guide.pdf). 

## System Requirements

UPC++ makes aggressive use of template meta-programming techniques, and
requires a modern C++11/14 compiler and corresponding STL implementation.

The current release is known to work on the following configurations:


* Mac OS X 10.11, 10.12 and 10.13 (El Capitan, Sierra and High Sierra,
 respectively) with the most recent Xcode releases for each, though it is
 suspected that any Xcode release 8.0 or newer will work.  
 (smp and udp conduits)

* Linux/x86-64 with gcc-5.1.0 or newer, or with clang-3.7.0 or newer when
 using libstdc++ from gcc-5.1.0.  If your system compilers do not meet these
 requirements, please see the note in [docs/local-gcc.md](docs/local-gcc.md)
 regarding use of non-system compilers.  
 (smp, udp and ibv conduits)

* Cray XC with PrgEnv-gnu and gcc/5.2.0 (or later) environment modules loaded.  
 (smp and aries conduits)

## Installation

For instructions on installing UPC++ and compiling programs, look at [INSTALL.md](INSTALL.md).

## Using UPC++ with MPI

For guidance on using UPC++ and MPI in the same applciation, see [docs/hybrid.md](docs/hybrid.md).

## ChangeLog

### 2017.09.30: Release 2017.9.0 

The initial public release of UPC++ v1.0. This release supports most of the
functionality specified in the UPC++ 1.0 draft 4 specification (located in the
docs directory). 

The following features from that specification are not yet implemented:

 * Continuation-based and Promise-based completion (use future completion for now)
 * Teams
 * Serialization
 * Non-contiguous transfers

This release is not performant, and may be unstable or buggy.

Please report any problems in the [issue tracker](https://bitbucket.org/berkeleylab/upcxx/issues).

### 2017.09.01: Release v1.0-pre

This is a prerelease of v1.0. This prerelease supports most of the
functionality covered in the UPC++ specification, except personas, promise-based completion, teams,
serialization, and non-contiguous transfers. This prerelease is not performant, and may be unstable
or buggy. Please notify us of issues by sending email to `upcxx-spec@googlegroups.com`.

