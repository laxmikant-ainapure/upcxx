### UPC\+\+: a PGAS extension for C\+\+ ###

UPC++ is a parallel programming extension for developing C++ applications with
the partitioned global address space (PGAS) model.  UPC++ has three main
objectives:

* Provide an object-oriented PGAS programming model in the context of the
  popular C++ language

* Expose useful asynchronous parallel programming idioms unavailable in
  traditional SPMD models, such as remote function invocation and
  continuation-based operation completion, to support complex scientific
  applications
 
* Offer an easy on-ramp to PGAS programming through interoperability with other
  existing parallel programming systems (e.g., MPI, OpenMP, CUDA)

For a description of how to use UPC++, please refer to the
[programmer's guide](docs/guide.pdf).

## System Requirements

UPC++ makes aggressive use of template meta-programming techniques, and requires
a modern C++11/14 compiler and corresponding STL implementation.

The current release is known to work on the following configurations:

* macOS 10.11, 10.12 and 10.13 (El Capitan, Sierra and High Sierra,
  respectively) with the most recent Xcode releases for each, though it is
  suspected that any Xcode (ie Apple clang) release 8.0 or newer will work. 
  Free Software Foundation GCC (e.g., as installed by Homebrew or Fink)
  version 5.1.0 or newer should also work (smp and udp conduits)

* Linux/x86-64 with one of the following compilers:    
   - Gnu g++ 5.1.0 or newer    
   - clang 3.7.0 or newer (with libstdc++ from gcc-5.1.0 or newer)    
   - Intel C++ 17.0.2 or newer (with libstdc++ from gcc-5.1.0 or newer)    
  If your system compilers do not meet these requirements, please see the note
  in [docs/local-gcc.md](docs/local-gcc.md) regarding use of non-system
  compilers. (smp, udp and ibv conduits)

* Linux/ppc64le with gcc-5.1.0 or newer (and see the note immediately above if
  you use a non-system compiler).

* Cray XC with the PrgEnv-gnu or PrgEnv-intel environment modules, as well as
  gcc/5.2.0 (or later) loaded. (smp and aries conduits)

Miscellaneous software requirements:

* Python2 version 2.7.5 or newer

* Perl version 5.005 or newer

* GNU Bash (must be installed, user's shell doesn't matter)

* Make (we recommend GNU make version 3.79 or newer).

* The following standard Unix tools: 'awk', 'sed', 'env', 'basename', 'dirname'

## Installation

For instructions on installing UPC++ and compiling programs, look at
[INSTALL.md](INSTALL.md).

## Using UPC++ with MPI

For guidance on using UPC++ and MPI in the same application, see
[docs/hybrid.md](docs/hybrid.md).

## Debugging

For recommendations on debugging, see [docs/debugging.md](docs/debugging.md)

## Testing

To run a test script, see [docs/testing.md](docs/testing.md)

## Legal terms

For copyright notice and licensing agreement, see [LICENSE.txt](LICENSE.txt)

## ChangeLog

### 2018.??.??: PENDING

* Team construction and collectives are nearly fully implemented. They currently
  lack support for any completions other than the default
  `operation_cx::as_future()`.

This release of UPC++ v1.0 supports most of the functionality specified in the 
[UPC++ 1.0 Draft ?? Specification](docs/spec.pdf).

New features/enhancements:

* New `upcxx` compiler wrapper makes it easier to build UPC++ programs
* UPC++ now supports the GCC compiler on macOS (e.g., as installed by Homebrew or Fink)
* rput_{strided,(ir)regular) now provide asynchronous source completion
* UPC++ library now contains ident strings that can be used to query version info
  from a compiled executable, using the UNIX `ident` tool.

The following features from the specification are not yet implemented:

...

Notable bug fixes:

* issue #49: stability and portability issues caused by C++ thread_local
* issue #141: missing promise move assignment operator

Please report any problems in the [issue tracker](https://bitbucket.org/berkeleylab/upcxx/issues).

### 2018.05.10: Release 2018.3.2

This is a re-release of version 2018.3.0 (see below) that corrects a packaging error.

### 2018.03.31: Release 2018.3.0

This release of UPC++ v1.0 supports most of the functionality specified in the 
[UPC++ 1.0 Draft 6 Specification](docs/spec.pdf).

New features/enhancements:

 * Non-Contiguous One-Sided RMA interface is now fully implemented.
 * Remote Atomics have been revamped, expanded and implemented. See the updated specification
   for usage details.  The current implementation leverages hardware support in
   shared memory and NIC offload support in Cray Aries.
 * View-Based Serialization - see the specification for details
 * Implementation of local memory translation (achieved with
   `global_ptr::local()` / `global_ptr(T*)`). This encompasses a limited
   implementation of teams to support `upcxx::world` and `upcxx::local_team`
   so clients may query their local neighborhood of ranks.

The following features from the specification are not yet implemented:

 * Teams: `team::split`, `team_id`, collectives over teams, passing
       `team&` arguments to rpcs, constructing `dist_object` over teams.
 * Vector broadcast `broadcast(T *buf, size_t count, ...)`
 * `barrier_async`
 * User-defined Serialization interface

Notable bug fixes:

 * issue 119: Build system is now more robust to GASNet-EX download failures.
 * issue 125: Fix upcxx-run exit code handling.
 * Minor improvements to upcxx-run and run-tests.

This release is not yet performant, and may be unstable or buggy.

Please report any problems in the [issue tracker](https://bitbucket.org/berkeleylab/upcxx/issues).

### 2018.01.31: Release 2018.1.0 BETA

This is a BETA preview release of UPC++ v1.0. This release supports most of the
functionality specified in the [UPC++ 1.0 Draft 5 Specification](https://bitbucket.org/upcxx/upcxx/downloads/upcxx-spec-V1.0-Draft5.pdf).

New features/enhancements:

 * Generalized completion. This allows the application to be notified about the
   status of UPC\+\+ operations in a handful of ways. For each event, the user
   is free to choose among: futures, promises, callbacks, delivery of remote
   rpc, and in some cases even blocking until the event has occurred.
 * Internal use of lock-free datastructures for `lpc` queues.
     * Enabled by default. See [INSTALL.md](INSTALL.md) for instructions on how
       to build UPC\+\+ with the older lock-based datastructure.
 * Improvements to the `upcxx-run` command.
 * Improvements to internal assertion checking and diagnostics.
  
The following features from that specification are not yet implemented:

 * Teams
 * Vector broadcast `broadcast(T *buf, size_t count, ...)`
 * `barrier_async`
 * Serialization
 * Non-contiguous transfers
 * Atomics

This release is not performant, and may be unstable or buggy.

Please report any problems in the [issue tracker](https://bitbucket.org/berkeleylab/upcxx/issues).

### 2017.09.30: Release 2017.9.0

The initial public release of UPC++ v1.0. This release supports most of the
functionality specified in the [UPC++ 1.0 Draft 4 Specification](https://bitbucket.org/upcxx/upcxx/downloads/upcxx-spec-V1.0-Draft4.pdf).

The following features from that specification are not yet implemented:

 * Continuation-based and Promise-based completion (use future completion for
   now)
 * `rput_then_rpc`
 * Teams
 * Vector broadcast `broadcast(T *buf, size_t count, ...)`
 * `barrier_async`
 * Serialization
 * Non-contiguous transfers

This release is not performant, and may be unstable or buggy.

Please report any problems in the [issue tracker](https://bitbucket.org/berkeleylab/upcxx/issues).

### 2017.09.01: Release v1.0-pre

This is a prerelease of v1.0. This prerelease supports most of the functionality
covered in the UPC++ specification, except personas, promise-based completion,
teams, serialization, and non-contiguous transfers. This prerelease is not
performant, and may be unstable or buggy. Please notify us of issues by sending
email to `upcxx@googlegroups.com`.

