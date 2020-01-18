# Developing and Maintaining UPC\+\+

These instructions are for UPC\+\+ runtime developers only.

THIS INTERNAL DOCUMENTATION IS NOT CAREFULLY MAINTAINED AND MAY BE OUT OF DATE.

Software requirements are detailed in [README.md](../README.md).  
Because we do not employ autoconf, automake or CMake, the requirements for
developers of UPC\+\+ are no different than for the end-users.  However, the
manner in which the tools might be used does differ.

## Workflow

The first thing to do when beginning a session of upcxx hacking is to
configure your build tree.  This can be done as many times as needed, for
instance using distinct build trees for distinct compilers (and sharing the
source tree).  Just run `<upcxx-src-path>/configure ...` in your build
directory (which *is* permitted to be the same as `<upcxx-src-path>`).
This will "capture" the options `--with-cc=...`, `--with-cxx=...`,
`--with-cross=...` and `--with-gasnet=...`.  Unless you re-run `configure`
those four parameters cannot be changed for a given build directory (but
you can have as many build directories as you want/need).

```
mkdir <upcxx-build-path>
cd <upcxx-build-path>
<upcxx-source-path>/configure --with-cc=... --with-cxx=... [--with-cross=...]
```

The legacy environment variables `CC`, `CXX`, `CROSS` and `GASNET` are still
available, but their use is deprecated.  Additionally, the legacy variable
`GASNET_CONFIGURE_ARGS` is still honored, but all unrecognized options to
the UPC\+\+ configure script are appended to it (giving them precedence).

[INSTALL.md](../INSTALL.md) has more information on the `configure` options
supported for end-users while "Internal-Only Configuration Options", below,
documents some unsupported ones.

The `configure` script populates `<upcxx-build-dir>/bin` with special `upcxx`,
`upcxx-meta` and `upcxx-run` scripts specific to use in the build-dir.  These
scripts dynamically build any necessary UPC\+\+ and GASNet-EX libraries the
first time they are required, and updates them if any UPC\+\+ or GASNet-EX
source files change.  For `upcxx` the necessary libraries are determined from
the environment as overridden by any flags passed to it.  For `upcxx-run`, the
conduit is extracted from the executable passed to it.  For `upcxx-meta` only
the environment is used.

These tools are expected to be sufficient for most simple development tasks,
including working with a user's bug reproducer (even one with its own Makefile),
without the need to complete an install with two versions of GASNet-EX and
`libupcxx.a` for every detected conduit.

By default, these three scripts remind you that you are using their build-dir
versions with the following message on stderr:  
      `INFO: may need to build the required runtime.  Please be patient.`  
This can be suppressed by setting `UPCXX_QUIET=1` in your environment.

In addition to the scripts in `<upcxx-build-dir>/bin`, there are a series
of make targets which honor the following nobs-inspired environment variables
(which can also be specified on the make command line):  

* `DBGSYM={0,1}`
* `ASSERT={0,1}`
* `OPTLEV={0,3}`
* `UPCXX_BACKEND=gasnet_{seq,par}`
* `GASNET_CONDUIT=...`

Note that unlike with `nobs`, the variables `CC`, `CXX`, `CROSS` and `GASNET`
are *not* honored by `make` because their values were "frozen" when
`configure` was run.

The make targets utilizing the variables above:

* `make exe SRC=foo.cpp [EXTRAFLAGS=-Dfoo=bar]`  
  Builds the given test printing the full path of the executable on stdout.  
  Executables are cached, including sensitivity to `EXTRAFLAGS` and to changes
  made to the `SRC` file, its crawled dependencies, and to UPC\+\+ source files.
* `make run SRC=foo.cpp [EXTRAFLAGS=-Dfoo=bar] [ARGS='arg1 arg2'] [RANKS=n]`  
  Builds and runs the given test with the optional arguments.  
  Executables are cached just as with `exe`.

* `make upcxx`, `make upcxx-run` and `make upcxx-meta`  
  Ensures the required libraries are built (and up-to-date) and prints to stdout
  the full path to an appropriate script specific to the current environment.

* `make upcxx-single`  
  Builds a single instance of `libupcxx.a` along with its required GASNet-EX
  conduit and an associated bottom-level `upcxx-meta`.  Use of this target
  with `-j<N>` can be useful to "bootstrap" the utilities in `bin`, which
  might otherwise build their prerequisites on-demand *without* parallelism.
* `make gasnet-single`  
  Builds a single GASNet-EX conduit.

The `exe` and `run` targets accept both absolute and relative paths for `SRC`.
Additionally, if the `SRC` value appears to be a relative path, but does not
exist, a search is conducted in the `test`, `example` and `bench` directories
within `<upcxx-src-path>`.  So, `make run SRC=hello_upcxx.cpp` and `make run
SRC=issue138.cpp` both "just work" without the need to type anything so
cumbersome as `<upcxx-src-path>/test/regression/issue138.cpp`.

Note that while bash syntax allows the following sort of incantation:  
    `X=$(DBGSYM=1 ASSERT=1 OPTLEV=0 echo $(make exe SRC=hello_gasnet.cpp))`  
passing of the environment variables on the make command line can simplify this
slightly to:  
    `X=$(make exe DBGSYM=1 ASSERT=1 OPTLEV=0 SRC=hello_gasnet.cpp)`

All make targets described here (as well as those documented for the end-user
in [INSTALL.md](../INSTALL.md)) are intented to be parallel-make-safe (Eg `make
-j<N> ...`).  Should you experience a failure with a parallel make, please
report it as a bug.
  
## Internal-Only Configuration Options

This serves as the place to document configure options that aren't hardened
enough to be part of the user-facing docs.

* `--with-mpsc-queue={atomic,biglock}`: The implementation to use for multi-
  producer single-consumer queues in the runtime (`upcxx::detail::intru_queue`).
    * `atomic`: (default) Highest performance: one atomic exchange per enqueue.
    * `biglock`: Naive global-mutex protected linked list. Low performance, but
      least risk with respect to potential bugs in implementation.
  The legacy environment variable `UPCXX_MPSC_QUEUE` is still honored at
  configure-time, but this behavior is deprecated.

* `--enable-single={debug,opt}`:  This limits the scope of make targets to only
  a single GASNet-EX build tree.  This has the side-effect of permitting (but not
  requiring) `--with-gasnet=...` to name an existing external GASNet-EX *build*
  tree (it must otherwise name a *source* tree or tarball).  
  This option is really only intended for use by our CI infrastructure, which
  operates on/with GASNet-EX build trees.  Since attempts to do anything outside
  the "scope" of the single-mode will likely fail in unexpected ways, this mode
  is likely to be more of an annoyance than an advantage to developers, other
  than when it is necessary to reproduce the CI environment.

* `-v` or `--verbose`:  This option is intended to support debugging of
  `configure` itself.  This should be the first command line option if one
  desires to debug option parsing, since it invokes `set -x` when this option is
  processed.

## Guide to Maintenance Tasks

#### To add a UPC\+\+ runtime source file

A list of sources to be compiled into `libupcxx.a` is maintained in
`bld/sources.mak`.  Simply add new library sources to `libupcxx_sources`.

#### To add a UPC\+\+ header file

Header files are "crawled" for dependency information, and at installation a
crawl rooted at `src/upcxx_headers.cpp` is used.  So, in general it is not
necessary to add new header files to any manually-maintained list.  If there are
headers missing from an install, then it is appropriate to update
`src/upcxx_headers.cpp` to ensure then are reached in the crawl.

#### To add tests to `make check` and `make tests`

Keep in mind that these targets are the ones we advise end-users (including
auditors) to run.  Tests that are not stable/reliable should not be added.
If/when it is appropriate to add a new test, it should be added to either
`testprograms_seq` or `testprograms_par` (depending on the backend it should be
built with) in `bld/tests.mak`.

#### Add a new GASNet-EX conduit

UPC\+\+ maintains its own list of supported conduits, allowing this to remain
a subset of GASNet-EX's full list as well as ensuring some targets (like
`tests-clean`) operate even when a build of GASNet-EX is not complete.  To add
to (or remove from) this list, edit `ALL_CONDUITS` in `bld/gasnet.mak`.

#### Testing GASNet-EX changes

Because all make targets include GASNet source files in their dependency
tracking, use of the Workflow described above can also be applied when
developing GASNet-level fixes to problems with UPC\+\+ reproducers.  If
necessary `make echovar VARNAME=GASNET` can be used to determine the GASNet
source directory in use (potentially created by `configure`).

#### To add a new supported compiler family

Currently, logic to check for supported compiler (family and version checks)
still lives in `utils/system-checks.sh`.

However, build-related configuration specific to the compiler family is kept in
`bld/compiler.mak`.  Current configuration variables in that file provide
documentation of their purpose, as well as some good examples of how they can
be set conditionally.
