### UPC\+\+: a PGAS extension for C\+\+ ###

UPC++ is a parallel programming extension for developing C++ applications with the partitioned
global address space (PGAS) model.  UPC++ has three main objectives:

* Provide an object-oriented PGAS programming model in the context of the popular C++ language

* Add useful parallel programming idioms unavailable in Unified Parallel C (UPC), such as
  asynchronous remote function invocation and multidimensional arrays, to support complex scientific
  applications
 
* Offer an easy on-ramp to PGAS programming through interoperability with other existing parallel
  programming systems (e.g., MPI, OpenMP, CUDA)

For a description of how to use UPC++, please refer to the [programmer's guide](docs/guide/guide.pdf). 

## System Requirements

UPC++ makes aggressive use of template meta-programming techniques, and
requires a modern C++11/14 compiler and corresponding STL implementation.

The current release is known to work on the following configurations:


* Mac OS X 10.11, 10.12 and 10.13beta (El Capitan, Sierra and High Sierra,
 respectively) with the most recent Xcode releases for each, though it is
 suspected that any Xcode release 8.0 or newer will work. 
 [smp and udp conduits]

* Linux/x86-64 with gcc-5.1.0 or newer, or with clang-3.7.0 when using
 libstdc++ from gcc-5.1.0 (but see the note [to be provided] about use of
 non-system compilers). 
 [smp, udp and ibv conduits]

* Cray XC with PrgEnv-gnu and gcc/5.2.0 (or later) environment modules loaded
 [smp and aries conduits]

## ChangeLog

### 2017.09.01: Release v1.0-pre

This is a prerelease of v1.0. This prerelease supports most of the
functionality covered in the UPC++ specification, except personas, promise-based completion, teams,
serialization, and non-contiguous transfers. This prerelease is not performant, and may be unstable
or buggy. Please notify us of issues by sending email to `upcxx-spec@googlegroups.com`.

