### UPC\+\+: a PGAS extension for C\+\+ ###

UPC++ is a parallel programming extension for developing C++ applications with the partitioned
global address space (PGAS) model.  UPC++ has three main objectives:

* Provide an object-oriented PGAS programming model in the context of the popular C++ language

* Add useful parallel programming idioms unavailable in Unified Parallel C (UPC), such as
  asynchronous remote function invocation and multidimensional arrays, to support complex scientific
  applications
 
* Offer an easy on-ramp to PGAS programming through interoperability with other existing parallel
  programming systems (e.g., MPI, OpenMP, CUDA)

### Release v1.0-pre

This is a prerelease of v1.0. The release date is 9/1/2017. This prerelease supports most of the
functionality covered in the UPC++ specification, except personas, promise-based completion, teams,
serialization, and non-contiguous transfers. This prerelease is not performant, and may be unstable
or buggy. Please notify us of issues by sending email to `upcxx-spec@googlegroups.com`.

For a description of how to use UPC++, please refer to the programmer's guide, at `doc/guide/`. 
