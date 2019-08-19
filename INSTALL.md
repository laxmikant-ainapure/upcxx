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
`rm -rf <upcxx-install-path>`.  Depending on the platform, additional
configuration may be necessary before invoking `install`. See below.

If you are using a source tarball release downloaded from the website, it
should include an embedded copy of GASNet-EX and `install` will default to
using that.  However if you are using a git clone or other repo snapshot of
UPC++, then `install` may default to downloading the GASNet-EX communication
library, in which case an Internet connection is needed at install time.

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
i.e. `CROSS=cray-aries-slurm`.

When Intel compilers are being
used (usually the default for these systems), a gcc environment module (6.4.0
or newer) is also required, and may need to be explicitly loaded, e.g.:

```bash
module load gcc/7.1.0
cd <upcxx-source-path>
CROSS=cray-aries-slurm ./install <upcxx-install-path>
```

If using PrgEnv-cray, then version 9.0 or newer of the Cray compilers is
required.  This means the cce/9.0.0 or later environment module must be
loaded, and not "cce/9.0.0-classic" (the "-classic" Cray compilers are not
supported).

The installer will use the `cc` and `CC` compiler aliases of the Cray
programming environment loaded.

Currently only Intel-based Cray XC systems have been tested, including Xeon
and Xeon Phi (aka "KNL").  Note that UPC++ has not yet been tested on an
ARM-based Cray XC.

**Installation: CUDA GPU support**

UPC++ now includes *prototype* support for communication operations on memory buffers
resident in a CUDA-compatible NVIDIA GPU. 
Note the CUDA support in this UPC++ release is a proof-of-concept reference implementation
which has not been tuned for performance. In particular, the current implementation of
`upcxx::copy` does not utilize hardware offload and is expected to underperform 
relative to solutions using RDMA, GPUDirect and similar technologies.
Performance will improve in an upcoming release.

System Requirements:

* NVIDIA-branded [CUDA-compatible GPU hardware](https://developer.nvidia.com/cuda-gpus)
* NVIDIA CUDA toolkit v9.0 or later. Available for [download here](https://developer.nvidia.com/cuda-downloads).

To activate the UPC++ support for CUDA, set environment variable `UPCXX_CUDA=1`
when running the install script:

```bash
cd <upcxx-source-path>
env UPCXX_CUDA=1 ./install <upcxx-install-path>
```

This expects to find the NVIDIA `nvcc` compiler wrapper in your `$PATH` and
will attempt to extract the correct build settings for your system.  If this
automatic extraction fails (resulting in preprocessor or linker errors
mentioning CUDA), then you may need to manually override the following
settings:

* `UPCXX_CUDA_NVCC`: the full path to the `nvcc` compiler wrapper from the CUDA toolkit. 
   Eg `UPCXX_CUDA_NVCC=/Developer/NVIDIA/CUDA-10.0/bin/nvcc`
* `UPCXX_CUDA_CPPFLAGS`: preprocessor flags to add for locating the CUDA toolkit headers.
   Eg `UPCXX_CUDA_CPPFLAGS='-I/Developer/NVIDIA/CUDA-10.0/include'`
* `UPCXX_CUDA_LIBFLAGS`: linker flags to use for linking CUDA executables.
   Eg `UPCXX_CUDA_LIBFLAGS='-Xlinker -force_load -Xlinker /Developer/NVIDIA/CUDA-10.0/lib/libcudart_static.a -L/Developer/NVIDIA/CUDA-10.0/lib -lcudadevrt -Xlinker -rpath -Xlinker /usr/local/cuda/lib -Xlinker -framework -Xlinker CoreFoundation -framework CUDA'`

Note that you must build UPC++ with the same host compiler toolchain as is used
by nvcc when compiling any UPC++ CUDA programs. That is, both UPC++ and your UPC++
application must be compiled using the same host compiler toolchain.
You can ensure this is the case by either (1) compiling UPC++ with the same
compiler as your system nvcc uses, or (2) using the `-ccbin` command line
argument to nvcc during application compilation to ensure it uses the same host
compiler as was used during UPC++ installation.
   
UPC++ CUDA operation can be validated using the following programs in the source tree:

* `test/copy.cpp`: correctness tester for the UPC++ `cuda_device`
* `bench/cuda_microbenchmark.cpp`: performance microbenchmark for `upcxx::copy` using GPU memory
* `example/cuda_vecadd`: demonstration of using UPC++ `cuda_device` to orchestrate
  communication for a program invoking CUDA computational kernels on the GPU.

See the "Memory Kinds" section in the _UPC++ Programmer's Guide_ for more details on 
using the CUDA support.

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
  Defaults to an embedded copy of GASNet-EX, or the GASNet-EX download URL.
* `GASNET_CONFIGURE_ARGS`: List of additional command line arguments passed to
  GASNet's configure phase.
* `UPCXX_PYTHON`: Python2 interpreter to use.
* `UPCXX_MAKE`: GNU Make command for make steps in the installation (e.g. building
  the GASNet-EX library). Defaults to `make -j 8` for parallel make.  To disable
  parallel make, set `UPCXX_MAKE=make`.
* `UPCXX_NOBS_THREADS`: Number of tasks to used for nobs-based compilation of the runtime.
  Defaults to autodetected CPU count, set `UPCXX_NOBS_THREADS=1` for serial compilation.

By default ibv-conduit (InfiniBand support) will use MPI for job spawning if a
working `mpicc` is found at installation time.  When this occurs, `CXX=mpicxx`
(or similar) is recommended at install time to ensure correct linkage of
ibv-conduit executables.  Alternatively, one may include `--disable-mpi-compat`
in the value of `GASNET_CONFIGURE_ARGS` to exclude support for MPI as a job
spawner.

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
