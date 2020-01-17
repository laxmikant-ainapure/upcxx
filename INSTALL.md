# UPC\+\+ Installation #

This file documents software installation of [UPC++](https://upcxx.lbl.gov).

For information on using UPC++, see: [README.md](README.md)    

## System Requirements

UPC++ makes aggressive use of template meta-programming techniques, and requires
a modern C++11/14 compiler and corresponding STL implementation.

The current release is known to work on the following configurations:

* macOS 10.11-10.15 (El Capitan, Sierra, High Sierra, Mojave or Catalina)
  with the most recent Xcode releases for each, though it is
  suspected that any Xcode (ie Apple clang) release 8.0 or newer will work. 
  Free Software Foundation GCC (e.g., as installed by Homebrew or Fink)
  version 6.4.0 or newer should also work (smp and udp conduits)

* Linux/x86\_64 with one of the following compilers:
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

* Cray XC x86\_64 with one of the following PrgEnv environment modules and
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

### Linux Compiler Notes:

* If /usr/bin/g++ is older than 6.4.0 (even if using a different C++
  compiler for UPC++) please read [docs/local-gcc.md](docs/local-gcc.md).

* If using a non-GNU compiler with /usr/bin/g++ older than 6.4.0, please also
  read [docs/alt-compilers.md](docs/alt-compilers.md).

### Miscellaneous software requirements:

* Python2 version 2.7.5 or newer

* Perl version 5.005 or newer

* GNU Bash 3.2 or newer (must be installed, user's shell doesn't matter)

* Make (we recommend GNU make version 3.79 or newer).

* The following standard Unix tools: 'awk', 'sed', 'env', 'basename', 'dirname'

## Installation Instructions

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

### Installation: Linux

The installation command above will work as is. The default compilers used will
be gcc/g++. The `CC` and `CXX` environment variables can be set to alternatives
to override this behavior. Additional environment variables allowing finer
control over how UPC\+\+ is configured can be found in the
"Advanced Installer Configuration" section below.

By default ibv-conduit (InfiniBand support) will use MPI for job spawning if a
working `mpicc` is found at installation time.  When this occurs, `CXX=mpicxx`
(or similar) is required at install time to ensure correct linkage of
ibv-conduit executables.  Alternatively, one may include `--disable-mpi-compat`
in the value of `GASNET_CONFIGURE_ARGS` to exclude support for MPI as a job
spawner.

### Installation: Apple macOS

On macOS, UPC++ defaults to using the Apple LLVM clang compiler that is part
of the Xcode Command Line Tools.

The Xcode Command Line Tools need to be installed *before* invoking `install`,
i.e.:

```bash
xcode-select --install
```

Alternatively, the `CC` and `CXX` environment variables can be set to an alternative
compiler installation for a supported compiler before running `./install`.

### Installation: Cray XC

To run on the compute nodes of a Cray XC, the `CROSS` environment variable needs
to be set before the install command is invoked. Use the appropriate value for
your supercomputer installation:

* `CROSS=cray-aries-slurm`: Cray XC systems using the SLURM job scheduler (srun)
* `CROSS=cray-aries-alps`: Cray XC systems using the Cray ALPS job scheduler (aprun)

When Intel compilers are being used (usually the default for these systems), 
a gcc environment module (6.4.0 or newer) is also required, and may need to be
explicitly loaded, e.g.:

```bash
module load gcc/7.1.0
cd <upcxx-source-path>
env CROSS=cray-aries-slurm ./install <upcxx-install-path>
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

### Installation: CUDA GPU support

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

## Advanced Installer Configuration

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
* `UPCXX_MAKE`: Make command for make steps in the installation (e.g. building
  the GASNet-EX library). Defaults to `make -j 8` for parallel make.  To disable
  parallel make, set `UPCXX_MAKE=make`.
* `UPCXX_NOBS_THREADS`: Number of tasks to used for non-make installation steps.
  Defaults to autodetected CPU count, set `UPCXX_NOBS_THREADS=1` for serial compilation.

