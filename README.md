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
    - PGI C++ 19.1 or newer (with libstdc++ from gcc-6.4.0 or newer)    

    See "Linux Compiler Notes", below, if `/usr/bin/g++` is older than 6.4.0.

* Linux/ppc64le with one of the following compilers:
    - gcc-6.4.0 or newer
    - clang 5.0.0 or newer (with libstdc++ from gcc-6.4.0 or newer)    
    - PGI C++ 18.10 or newer (with libstdc++ from gcc-6.4.0 or newer)    

    See "Linux Compiler Notes", below, if `/usr/bin/g++` is older than 6.4.0.

* Linux/aarch64 (aka "arm64" or "armv8") with one of the following compilers:
    - gcc-6.4.0 or newer
    - clang 4.0.0 or newer (with libstdc++ from gcc-6.4.0 or newer)   

    See "Linux Compiler Notes", below, if `/usr/bin/g++` is older than 6.4.0.

    Note that gcc- and clang-based toolchains from Arm Ltd. exist, but have
    not been tested with UPC++.

    Support for InfiniBand on Linux/aarch64 should be considered experimental.
    For more information, please see
    [GASNet bug 3997](https://upc-bugs.lbl.gov/bugzilla/show_bug.cgi?id=3997).

* Cray XC x86_64 with one of the following PrgEnv environment modules and
  its dependencies.  (smp and aries conduits)
    - PrgEnv-gnu with gcc/6.4.0 (or later) loaded.
    - PrgEnv-intel with gcc/6.4.0 (or later) loaded.
    - PrgEnv-cray with cce/9.0.0 (or later) loaded.
      Note that does not include support for "cce/9.x.y-classic".

    ALCF's PrgEnv-llvm is also supported on the Cray XC.  Unlike Cray's
    PrgEnv-\* modules, PrgEnv-llvm is versioned to match the llvm toolchain
    it includes, rather than the Cray PE version.  UPC++ has been tested
    against PrgEnv-llvm/4.0 (clang 4.0) and newer.  When using PrgEnv-llvm,
    it is recommended to `module unload xalt` to avoid a large volume of
    verbose linker output in this configuration.  Mixing with OpenMP in this
    configuration is not currently supported.  (smp and aries conduits).

Linux Compiler Notes:

* If /usr/bin/g++ is older than 6.4.0 (even if using a different C++
  compiler for UPC++) please read [docs/local-gcc.md](docs/local-gcc.md).

* If using a non-GNU compiler with /usr/bin/g++ older than 6.4.0, please also
  read [docs/alt-compilers.md](docs/alt-compilers.md).

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

## Using UPC++ with CMake

A UPCXX CMake module is provided in the installation directory. To use it
in a CMake project, append the UPC++ installation directory to the
`CMAKE_PREFIX_PATH` variable 
(`cmake ... -DCMAKE_PREFIX_PATH=/path/to/upcxx/install/prefix ...`), 
then use `find_package(UPCXX)` in the
CMakeLists.txt file of the project.

If it is able to find a compatible UPC++ installation, the CMake module
will define a `UPCXX:upcxx target` (as well as a `UPCXX_LIBRARIES`
variable for legacy projects) that can be added as dependency to
your project.

## Implementation-defined behavior

[docs/implementation-defined.md](docs/implementation-defined.md) documents 
implementation-defined behaviors of this implementation.

## Legal terms

For copyright notice and licensing agreement, see [LICENSE.txt](LICENSE.txt)

