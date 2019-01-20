# UPC\+\+ Installation #

The general recipe for building and installing UPC\+\+ is to run the `install`
script found in the top-level (upcxx) source directory:

```bash
cd <upcxx-source-path>
./install <upcxx-install-path>
```

This will build the UPC\+\+ library and install it to the `<upcxx-install-path>`
directory. Users are recommended to use paths to non-existent or empty
directories as the installation path so that uninstallation is as trivial as
`rm -rf <upcxx-install-path>`.  Note that the install process downloads the
GASNet communication library, so an Internet connection is needed. Depending on
the platform, additional configuration may be necessary before invoking
`install`. See below.

Note: The install script requires Python 2.7 and does its best to automatically
invoke that version even if it isn't the current default on the system. If
installation fails and you suspect it has to do with the Python version, try
setting the `UPCXX_PYTHON` environment variable to refer to your system's name
for Python 2.7 (e.g. `python2.7`, or `python2`).

**Installation: Linux**

The installation command above will work as is. The default compilers used will
be gcc/g++. The `CC` and `CXX` environment variables can be set to alternatives
to override this behavior. Additional environment variables allowing finer
control over how UPC\+\+ is configured can be found in the
"Advanced Installer Configuration" section below.

**Installation: Apple macOS**

On macOS, UPC++ defaults to using the Apple LLVM clang compiler that is part
of the Xcode Command Line Tools.

The Xcode Command Line Tools need to be installed *before* invoking `install`,
i.e.:

```bash
xcode-select --install
```

Alternatively, the `CC` and `CXX` environment variables can be set to an alternative
compiler installation for a supported compiler before running `./install`.

**Installation: Cray XC**

To run on the compute nodes of a Cray XC, the `CROSS` environment variable needs
to be set before the install command is invoked,
i.e. `CROSS=cray-aries-slurm`. Additionally, if the Intel compilers are being
used (usually the default for these systems), the gcc/5.2.0 or newer module
must be loaded, e.g.:

```bash
module load gcc/7.1.0
cd <upcxx-source-path>
CROSS=cray-aries-slurm ./install <upcxx-install-path>
```

The installer will use the `cc` and `CC` compiler aliases of the Cray
programming environment loaded.

## Advanced Installer Configuration ##

The installer script tries to pick a sensible default behavior for the platform
it is running on, but the install can be customized using the following
environment variables:

* `CC`, `CXX`: The C and C\+\+ compilers to use.
* `CROSS`: The cross-configure settings script to pull from the GASNet source
  tree (computed as `<gasnet>/other/contrib/cross-configure-${CROSS}`).
* `GASNET`: Provides the GASNet-EX source tree from which the UPC\+\+ install
  script will build its own version of GASNet-EX. This can be a path to a tarball,
  URL to a tarball, or path to a full source tree. If provided, this must correspond 
  to a recent and compatible version of GASNet-EX (NOT GASNet-1).
  Defaults to a URL to a publicly available GASNet-EX tarball. 
* `GASNET_CONFIGURE_ARGS`: List of additional command line arguments passed to
  GASNet's configure phase.

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
you'll need to set the `UPCXX_GASNET_CONDUIT` or `UPCXX_THREADMODE` variables
prior to invoking compilation. See the `UPC++ Backends` section below.

# Compiling Against UPC\+\+ in Makefiles #

With UPC\+\+ installed, the application's build process can query for the
appropriate compiler flags to enable building against upcxx by invoking the
`<upcxx-install-path>/bin/upcxx-meta <what>` script, where `<what>` indicates
which form of flags are desired. Valid values are:

* `CPPFLAGS`: Preprocessor flags which will put the upcxx headers in the
  compiler's search path and define macros required by those headers.
* `CXXFLAGS`: (optional) Compiler flags which set debug/optimization settings.
* `LDFLAGS`: Linker flags usually belonging at the front of the link command
  line (before the list of object files).
* `LIBS`: Linker flags belonging at the end of the link command line. These
  will make libupcxx and its dependencies available to the linker.

For example, to build an application consisting of `my-app1.cpp` and
`my-app2.cpp`:

```bash
upcxx="<upcxx-install-path>/bin/upcxx-meta"
<c++ compiler> -std=c++11 $($upcxx CPPFLAGS) $($upcxx CXXFLAGS) -c my-app1.cpp
<c++ compiler> -std=c++11 $($upcxx CPPFLAGS) $($upcxx CXXFLAGS) -c my-app2.cpp
<c++ compiler> $($upcxx LDFLAGS) my-app1.o my-app2.o $($upcxx LIBS)
```

The `<c++ compiler>` used to build the application must be the same as the one
used for the UPC\+\+ installation.

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

* `UPCXX_GASNET_CONDUIT=[smp|udp|aries|ibv]`: The GASNet conduit to use (the
  default value is platform dependent):
    * `smp` is the typical high-performance choice for single-node multi-core
      runs .
    * `udp` is a useful low-performance alternative for testing and debugging. 
    * `aries` is the high-performance Cray XC network.
    * `ibv` is the high-performance InfiniBand network.

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
