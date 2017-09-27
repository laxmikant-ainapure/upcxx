# UPC\+\+ Installation #

The general recipe for building and installing UPC\+\+ is to run the
`install` script found in the top-level (upcxx) source directory:

```bash
cd <upcxx-source-path>
./install <upcxx-install-path>
```

This will build the UPC\+\+ library and install it to the 
`<upcxx-install-path>` directory. Users are recommended to use paths to 
non-existent or empty directories as the installation path so that 
uninstallation is as trivial as `rm -rf <upcxx-install-path>`.  Note 
that the install process downloads the GASNet communication library, so 
an Internet connection is needed. Depending on your platform, 
additional configuration may be necessary before invoking `install`. 
See below.

### Installation: Linux ###

The installation command above will work as is. The default compilers 
used will be gcc/g++. The `CC` and `CXX` environment variables can be 
set to alternatives to override this behavior. Additional environment 
variables allowing finer control over how UPC\+\+ is configured can be 
found further in this document.

### Installation: Mac ###

Be sure to install the Xcode Command Line Tools before invoking
`install`.

```bash
xcode-select --install

# and then the usual installation command
cd <upcxx-source-path>
./install <upcxx-install-path>
```

### Installation: Cray XC ###

To inform the installer of your intent of running on the compute nodes 
of a Cray XC, you must have `CROSS=cray-aries-slurm` in your 
environment when the installation is invoked. Additionally, because 
UPC\+\+ does not currently support the Intel compilers (usually the 
default for these systems) you must load in either GCC or Clang.

```bash
module switch PrgEnv-intel PrgEnv-gnu

cd <upcxx-source-path>
CROSS=cray-aries-slurm ./install <upcxx-install-path>
```

The installer will use the `cc` and `CC` compiler aliases of the Cray
programming environment loaded.

## Advanced Installer Configuration ##

The installer script tries to pick a sensible default behavior for the
platform it's running on, but a user wishing for more control over that
behavior may be interested in the following set of environment
variables.

* `CC`, `CXX`: The C and C\+\+ compilers to use.
* `CROSS`: The cross-configure settings script to pull from the GASNet 
  source tree (computed as 
`<gasnet>/other/contrib/cross-configure-${CROSS}`).
* `GASNET`: Provides the GASNetEx source tree from which the UPC\+\+ 
  install script will build its own version of GASNet. This can be a 
  path to a tarball, url to a tarball, or path to a full source tree. 
  Defaults to a url to a publicly available GASNetEx tarball.
* `GASNET_CONFIGURE_ARGS`: List of additional command line arguments
  passed to GASNet's configure phase.

## Compiling Against UPC\+\+ ##

With UPC\+\+ installed, your application's build process can query for
the appropriate compiler flags to enable building against upcxx by
invoking the `<upcxx-install-path>/bin/upcxx-meta <what>` script, where
`<what>` indicates which form of flags are desired. Valid values are:

* `PPFLAGS`: Preprocessor flags which will put the upcxx headers in 
  the compiler's search path and define macros required by those 
  headers.
* `LDFLAGS`: Linker flags usually belonging at the front of the link
  command line (before the list of object files).
* `LIBFLAGS`: Linker flags belonging at the end of the link command
  line. These will make libupcxx and its dependencies available to
  the linker.

For example, to build a single-file application `my-app.cpp` with
UPC\+\+:

```bash
<c++ compiler> -std=c++11 \
  $(<upcxx-install-path>/bin/upcxx-meta PPFLAGS) \
  my-app.cpp \
  $(<upcxx-install-path>/bin/upcxx-meta LDFLAGS) \
  $(<upcxx-install-path>/bin/upcxx-meta LIBFLAGS)
```

Building a multi-file application consisting of `my-app1.cpp` and
`my-app2.cpp` could look like:

```bash
upcxx="<upcxx-install-path>/bin/upcxx-meta"
<c++ compiler> -std=c++11 $($upcxx PPFLAGS) -c my-app1.cpp
<c++ compiler> -std=c++11 $($upcxx PPFLAGS) -c my-app2.cpp
<c++ compiler> $($upcxx LDFLAGS) my-app1.o my-app2.o $($upcxx LIBFLAGS)
```

Be sure that the `<c++ compiler>` used to build your application is the
same one UPC\+\+'s installation used.

For an example of a Makefile which builds UPC++ applications, look at
`example/prog-guide/Makefile`. This directory also has code for running
all the examples given in the guide. To use that `Makefile`, first set
the `UPCXX_INSTALL` shell variable to the `<upcxx-install-path>`.

# UPC\+\+ Backends #

UPC\+\+ provides multiple "backends" offering the user flexibility to 
choose the means by which the parallel communication facilities are 
implemented. At the moment, these backends are characterized by three 
dimensions: conduit, thread-mode, and code-mode. The conduit and 
thread-mode parameters map directly to the GASNet concepts of the same 
name. Code-mode selects between highly optimized code and highly 
debuggable code. The `upcxx-meta` script will assume sensible defaults 
for these paramters based on the installation configuration. The 
following environment variables can be set to influence which backend 
`upcxx-meta` selects.

* `UPCXX_GASNET_CONDUIT=[smp|udp|aries]`: The GASNet conduit to use. 
  `smp` is the typical high-performance choice for single-node 
  multi-core runs. `udp` is a useful low-performance alternative for 
  testing and debugging. And `aries` is the high-performance Cray XC 
  network. The default value is platform dependent.
* `UPCXX_THREADMODE=[seq|par]`: The value `seq` limits the 
  application to only calling "communicating" upcxx routines from the 
  thread that invoked `upcxx::init`, and only while that thread is 
  holding the master persona. The benefit is that `seq` can be 
  synchronization free in much of its internals. A thread-mode value of 
  `par` allows any thread in the process to issue communication as 
  allowed by the specification, allowing for greater injection 
  concurrency from a multi-threaded application but at the expensive of 
  greater internal synchronization (higher overheads per operation). 
  The default value is always `seq`.
* `UPCXX_CODEMODE=[O3|debug]`: `O3` is for highly compiler-optimized 
  code. `debug` produces unoptimized code, includes extra error 
  checking assertions, and is annotated with the symbol tables needed 
  by debuggers. The default value is always `O3`.

## Running UPC\+\+ Programs ##

To run a parallel UPC\+\+ application, use the `upcxx-run` launcher
provided in the installation directory:

```bash
<upcxx-install-path>/bin/upcxx-run <ranks> <exe> <args...>
```

This will run the executable and arguments `<exe> <args...>` in a 
parallel context with `<ranks>` number of UPC\+\+ ranks.
