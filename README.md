# UPC\+\+: a PGAS library for C\+\+ #

UPC++ is a parallel programming library for developing C++ applications with
the Partitioned Global Address Space (PGAS) model.  UPC++ has three main
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

* macOS 10.11-10.14 (El Capitan, Sierra, High Sierra, or Mojave)
  with the most recent Xcode releases for each, though it is
  suspected that any Xcode (ie Apple clang) release 8.0 or newer will work. 
  Free Software Foundation GCC (e.g., as installed by Homebrew or Fink)
  version 6.4.0 or newer should also work (smp and udp conduits)

* Linux/x86_64 with one of the following compilers:    
    - Gnu g++ 6.4.0 or newer    
    - clang 4.0.0 or newer (with libstdc++ from gcc-6.4.0 or newer)    
    - Intel C++ 17.0.2 or newer (with libstdc++ from gcc-6.4.0 or newer)    
  If your system compilers do not meet these requirements, please see the note
  in [docs/local-gcc.md](docs/local-gcc.md) regarding use of non-system
  compilers. (smp, udp and ibv conduits)

* Linux/ppc64le with one of the following compilers:
    - gcc-6.4.0 or newer (and see the note immediately above if
      you use a non-system compiler).
    - clang 5.0.0 or newer (with libstdc++ from gcc-6.4.0 or newer)    

* Cray XC x86_64 with the PrgEnv-gnu or PrgEnv-intel environment modules, 
  as well as gcc/6.4.0 (or later) loaded. (smp and aries conduits)

  ALCF's PrgEnv-llvm is also supported on the Cray XC.  Unlike Cray's
  PrgEnv-* modules, PrgEnv-llvm is versioned to match the llvm toolchain
  it includes, rather than the Cray PE version.  UPC++ has been tested
  against PrgEnv-llvm/4.0 (clang 4.0) and newer.  When using PrgEnv-llvm,
  it is recommended to `module unload xalt` to avoid a large volume of
  verbose linker output in this configuration.  Mixing with OpenMP in this
  configuration is not currently supported.  (smp and aries conduits).

Miscellaneous software requirements:

* Python2 version 2.7.5 or newer

* Perl version 5.005 or newer

* GNU Bash (must be installed, user's shell doesn't matter)

* Make (we recommend GNU make version 3.79 or newer).

* The following standard Unix tools: 'awk', 'sed', 'env', 'basename', 'dirname'

## Installation

For instructions on installing UPC++ and compiling programs, look at
[INSTALL.md](INSTALL.md).

## Debugging

For recommendations on debugging, see [docs/debugging.md](docs/debugging.md)

Please report any problems in the [issue tracker](https://upcxx-bugs.lbl.gov).

## Testing

To run a UPC++ correctness test, see [docs/testing.md](docs/testing.md)

## Using UPC++ with other programming models

**MPI**: For guidance on using UPC++ and MPI in the same application, see 
[docs/hybrid.md](docs/hybrid.md).

**UPC**: For guidance on using UPC++ and UPC in the same application, see 
[docs/upc-hybrid.md](docs/upc-hybrid.md).

## Implementation-defined behavior

[docs/implementation-defined.md](docs/implementation-defined.md) documents 
implementation-defined behaviors of this implementation.

## Legal terms

For copyright notice and licensing agreement, see [LICENSE.txt](LICENSE.txt)

## ChangeLog

### 2019.XX.XX: PENDING

New features/enhancements: (see specification and programmer's guide for full details)

* `atomic_domain<float>` and `atomic_domain<double>` are now implemented
* clang v5.0+ has been added to the list of supported compilers on Linux/ppc64le platforms
* New define `UPCXX_SPEC_VERSION` documents the implemented revision of the UPC++ specification

Notable bug fixes:

* issue #184: `bench/put_flood` crashes on opt/Linux
* issue #203: strict aliasing violations in device\_allocator
* issue #228: incorrect behavior for `upcxx -g -O`
* issue #229: Teach upcxx wrapper to compile C language files
* issue #224: missing const qualifier on `dist_object<T>.fetch()`
* issue #223: `operator<<(std::ostream, global_ptr<T>)` does not match spec

The following features from the specification are not yet implemented:

* `barrier_async()` and `broadcast()` only support default future-based completion (issue #234)
* `team_id::when_here()` is unimplemented (issue #170)
* User-defined Class Serialization interface (coming soon!)

Breaking changes:
* Applications are recommended to replace calls to `std::getenv` with `upcxx::getenv_console`,
  to maximize portability to loosely coupled distributed systems.

* envvar `UPCXX_GASNET_CONDUIT` has been renamed to `UPCXX_NETWORK`.
  For backwards compat, the former is still accepted when the latter is unset.

### 2019.05.27: Bug-fix release 2019.3.2

Notable bug fixes:

* issue #209: Broken install defaulting of CC/CXX on macOS

Fixes the following notable bug in the GASNet library
  (see https://gasnet-bugs.lbl.gov for details):

* bug3943: infrequent startup hang with PSHM and over 62 PPN

### 2019.03.15: Release 2019.3.0

This release of UPC++ v1.0 supports most of the functionality specified in the 
[UPC++ 1.0 Draft 10 Specification](docs/spec.pdf).

New features/enhancements: (see specification and programmer's guide for full details)

* Prototype Memory Kinds support for CUDA-based NVIDIA GPUs, see [INSTALL.md](INSTALL.md).
    Note the CUDA support in this UPC++ release is a proof-of-concept reference implementation
    which has not been tuned for performance. In particular, the current implementation of
    `upcxx::copy` does not utilize hardware offload and is expected to underperform 
    relative to solutions using RDMA, GPUDirect and similar technologies.
    Performance will improve in an upcoming release.
* Support for interoperability with Berkeley UPC, see [upc-hybrid.md](docs/upc-hybrid.md)
* There is now an offline installer package for UPC++, for systems lacking connectivity
* Barrier synchronization performance has been improved
* Installer now defaults to more build parallelism, improving efficiency (see `UPCXX_MAKE`)

The following features from the specification are not yet implemented:

* `barrier_async()` and `broadcast()` only support default future-based completion (issue #234)
* `atomic_domain<float>` and `atomic_domain<double>` are not yet implemented (issue #235)
* `team_id::when_here()` is unimplemented (issue #170)
* User-defined Class Serialization interface 

Notable bug fixes:

* issue #100: Fix shared heap setting propagation on loosely-coupled clusters
* issue #118: Enforce GEX version interlock at compile time
* issue #177: Completion broken for non-fetching binary AMOs
* issue #183: `bench/{put_flood,nebr_exchange}` were failing to compile
* issue #185: Fix argument order for `dist_object` constructor to match spec
* issue #187: Improve Python detection logic for the install script
* issue #190: Teach upcxx-run to honor `UPCXX_PYTHON`
* issue #202: Make `global_ptr::operator bool` conversion explicit 
* issue #205: incorrect metadata handling in `~persona_scope()`

Breaking changes:

* envvar `UPCXX_SEGMENT_MB` has been renamed to `UPCXX_SHARED_HEAP_SIZE`.
  For backwards compat, the former is still accepted when the latter is unset.
* The minimum-supported version of GNU g++ is now 6.4.0
    - This also applies to the stdlibc++ used by Clang or Intel compilers
* The minimum-supported version of llvm/clang for Linux is now 4.0

### 2018.09.26: Release 2018.9.0

This release of UPC++ v1.0 supports most of the functionality specified in the 
[UPC++ 1.0 Draft 8 Specification](https://bitbucket.org/berkeleylab/upcxx/downloads/upcxx-spec-V1.0-Draft8.pdf).

New features/enhancements: (see specification and programmer's guide for full details)

* Subset teams and team-aware APIs are added and implemented
* Non-Blocking Collective operations, with team support: barrier, broadcast, reduce
* New atomic operations: `mul, min, max, bit_{and,or,xor}`
* `future::{wait,result}*` return types are now "smarter", allowing more concise syntax
* New `upcxx` compiler wrapper makes it easier to build UPC++ programs
* `upcxx-run`: improved functionality and handling of -shared-heap arguments
* New supported platforms:
    - GNU g++ compiler on macOS (e.g., as installed by Homebrew or Fink)
    - PrgEnv-intel version 17.0.2 or later on Cray XC x86-64 systems
    - Intel C++ version 17.0.2 or later on x86-64/Linux
    - GNU g++ compiler on ppc64le/Linux
* `rput_{strided,(ir)regular}` now provide asynchronous source completion
* Performance improvements to futures, promises and LPCs
* UPC++ library now contains ident strings that can be used to query version info
  from a compiled executable, using the UNIX `ident` tool.

The following features from the specification are not yet implemented:

* Non-Blocking collectives currently support only the default future-based completion
* `atomic_domain<float>` and `atomic_domain<double>` are not yet implemented
* `team_id::when_here()` is unimplemented
* User-defined Serialization interface

Notable bug fixes:

* issue #49: stability and portability issues caused by C++ `thread_local`
* issue #141: missing promise move assignment operator

Breaking changes:

* `global_ptr<T>(T*)` "up-cast" constructor has been replaced with `to_global_ptr<T>(T*)`
* `atomic_domain` now requires a call to new collective `destroy()` before destructor
* `allreduce` has been renamed to `reduce_all`

### 2018.05.10: Release 2018.3.2

This is a re-release of version 2018.3.0 (see below) that corrects a packaging error.

### 2018.03.31: Release 2018.3.0

This release of UPC++ v1.0 supports most of the functionality specified in the 
[UPC++ 1.0 Draft 6 Specification](https://bitbucket.org/berkeleylab/upcxx/downloads/upcxx-spec-V1.0-Draft6.pdf).

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

Please report any problems in the [issue tracker](https://upcxx-bugs.lbl.gov).

### 2018.01.31: Release 2018.1.0 BETA

This is a BETA preview release of UPC++ v1.0. This release supports most of the
functionality specified in the [UPC++ 1.0 Draft 5 Specification](https://bitbucket.org/berkeleylab/upcxx/downloads/upcxx-spec-V1.0-Draft5.pdf).

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

### 2017.09.30: Release 2017.9.0

The initial public release of UPC++ v1.0. This release supports most of the
functionality specified in the [UPC++ 1.0 Draft 4 Specification](https://bitbucket.org/berkeleylab/upcxx/downloads/upcxx-spec-V1.0-Draft4.pdf).

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

### 2017.09.01: Release v1.0-pre

This is a prerelease of v1.0. This prerelease supports most of the functionality
covered in the UPC++ specification, except personas, promise-based completion,
teams, serialization, and non-contiguous transfers. This prerelease is not
performant, and may be unstable or buggy. Please notify us of issues by sending
email to `upcxx@googlegroups.com`.

