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

# Compiling Against UPC\+\+ on the Command Line #

With UPC\+\+ installed, the easiest way to build a UPC++ application from the
command line is to use the `upcxx` compiler wrapper, installed in 
`<upcxx-install-path>/bin/upcxx`. This arguments to this wrapper work
just like the C++ compiler used to install UPC++ (analogous to the
`mpicxx` compiler wrapper often provided for MPI/C++ programs).

For example, to build an application consisting of `my-app1.cpp` and
`my-app2.cpp`:

```bash
export PATH="<upcxx-install-path>/bin/:$PATH"
upcxx -O -c my-app1.cpp my-app2.cpp
upcxx -O -o my-app my-app1.o my-app2.o -lm
```

Be sure that all commands used to build one executable consistently pass either
a -O option to select the optimized/production version of UPC++ (for
performance runs), or a -g option to select the debugging version of UPC++
(for tracking down bugs in your application).

To select a non-default network backend or thread-safe version of the library, 
you'll need to pass the -network= or -threadmode= options, or set the
`UPCXX_NETWORK` or `UPCXX_THREADMODE` variables prior to invoking compilation.
See the `UPC++ Backends` section below.

# Compiling Against UPC\+\+ in Makefiles #

The simplest way to build UPC++ programs from a Makefile is to use the 
`upcxx` compiler wrapper documented in the section above to replace your
normal C++ compiler command.

If your Makefile structure prevents this and/or requires extraction of the 
underlying compiler flags to build against UPC++, your build process can 
query this information by invoking the
`<upcxx-install-path>/bin/upcxx-meta <what>` script, where `<what>` indicates
which form of flags are desired. Valid values are:

* `CXX`: The C++ compiler used to install UPC++, which must also be used for
  building application code.
* `CPPFLAGS`: Preprocessor flags which will put the upcxx headers in the
  compiler's search path and define macros required by those headers.
* `CXXFLAGS`: Compiler flags which set debug/optimization settings, and
  set the minimum C++ language level required by the UPC++ headers.
* `LDFLAGS`: Linker flags usually belonging at the front of the link command
  line (before the list of object files).
* `LIBS`: Linker flags belonging at the end of the link command line. These
  will make libupcxx and its dependencies available to the linker.

For example, to build an application consisting of `my-app1.cpp` and
`my-app2.cpp` using extracted arguments:

```bash
meta="<upcxx-install-path>/bin/upcxx-meta"
$($meta CXX) $($meta CPPFLAGS) $($meta CXXFLAGS) -c my-app1.cpp
$($meta CXX) $($meta CPPFLAGS) $($meta CXXFLAGS) -c my-app2.cpp
$($meta CXX) $($meta LDFLAGS) my-app1.o my-app2.o $($meta LIBS)
```

For an example of a Makefile which builds UPC++ applications, look at
[example/prog-guide/Makefile](example/prog-guide/Makefile). This directory also
has code for running all the examples given in the programmer's guide. To use
that `Makefile`, first set the `UPCXX_INSTALL` shell variable to the
`<upcxx-install-path>`.

## UPC\+\+ Backends ##

UPC\+\+ provides multiple "backends" offering the user flexibility to choose the
means by which the parallel communication facilities are implemented. Those
backends are characterized by three dimensions: conduit, thread-mode, and
code-mode. The conduit and thread-mode parameters map directly to the GASNet
concepts of the same name (for more explanation, see below). Code-mode selects
between highly optimized code and highly debuggable code. The `upcxx-meta`
script will assume sensible defaults for these parameters based on the
installation configuration. The following environment variables can be set to
influence which backend `upcxx-meta` selects:

* `UPCXX_NETWORK=[aries|ibv|smp|udp|mpi]`: The GASNet network backend to use
  for communication (the default and available values are system-dependent):
    * `aries` is the high-performance Cray XC network.
    * `ibv` is the high-performance InfiniBand network.
    * `smp` is the high-performance choice for single-node multi-core runs.
    * `udp` is a portable low-performance alternative for testing and debugging.
    * `mpi` is a portable low-performance alternative for testing and debugging. 

* `UPCXX_THREADMODE=[seq|par]`: The value `seq` limits the application to only
  calling "communicating" upcxx routines from the thread that invoked
  `upcxx::init`, and only while that thread is holding the master persona. The
  benefit is that `seq` can be synchronization free in much of its internals. A
  thread-mode value of `par` allows any thread in the process to issue
  communication as allowed by the specification, allowing for greater injection
  concurrency from a multi-threaded application but at the expensive of greater
  internal synchronization (higher overheads per operation).  The default value
  is always `seq`.
  
* `UPCXX_CODEMODE=[O3|debug]`: `O3` is for highly compiler-optimized
  code. `debug` produces unoptimized code, includes extra error checking
  assertions, and is annotated with the symbol tables needed by debuggers. The
  default value is always `O3`.

# Running UPC\+\+ Programs #

To run a parallel UPC\+\+ application, use the `upcxx-run` launcher provided in
the installation.

```bash
<upcxx-install-path>/bin/upcxx-run -n <ranks> <exe> <args...>
```

This will run the executable and arguments `<exe> <args...>` in a parallel
context with `<ranks>` number of UPC\+\+ ranks.

Upon startup, each UPC\+\+ rank creates a fixed-size shared memory heap that will never grow. By
default, this heap is 128 MB per rank. This can be adjust by passing a `-shared-heap` parameter
to the run script, which takes a suffix of KB, MB or GB; e.g. to reserve 1GB per rank, call:

```bash
<upcxx-install-path>/bin/upcxx-run -shared-heap 1G -n <ranks> <exe> <args...>
```

There are several additional options that can be passed to `upcxx-run`. Execute with `-h` to get a
list of options. 
