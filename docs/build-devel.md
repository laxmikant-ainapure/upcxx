# Building UPC\+\+ #

These instructions are for UPC++ runtime developers only.

THIS INTERNAL DOCUMENTATION IS NOT CAREFULLY MAINTAINED AND MAY BE OUT OF DATE.

Requirements:

  - Bash
  - Python 2.x >= 2.7.5
  - C++11 compiler: (gcc >= 4.9) or (clang >= ???) or (icc >= ???)

## Workflow ##

The first thing to do when beginning a session of upcxx hacking is to source the
`sourceme` bash script. This will populate your current bash environment with
the needed commands for developing and building upcxx.

```
cd <upcxx>
. sourceme
```

The buildsystem "nobs" is now availabe under the `nobs` bash function, but only
for this bash instance. First useful thing to try:

```
nobs exe test/hello_gasnet.cpp
```

This should download gasnet, build it, then build and link
"test/hello_gasnet.cpp" and eventually write some cryptic hash encrusted
filename to stdout. That is the path to our just built executable. At the moment
gasnet defaults to building in the SMP conduit, so to test out a parallel run do
this:

```
GASNET_PSHM_NODES=2 $(nobs exe test/hello_gasnet.cpp)
```

Or equivalently this convenience command which implicitly runs the executable
after building (comes in handy when you want nice paging of error messages in
the case of build failure, which nobs does really well):

```
GASNET_PSHM_NODES=2 nobs run test/hello_gasnet.cpp
```

All object files and executables and all other artifacts of the build process
are stored internally in "upcxx/.nobs/art". Their names are cryptic hashes so
don't bother looking around. To get the name of something you care about just
use bash code like `mything=$(nobs ...)`. The big benefit to managing our
artifacts this way is that we can keep our source tree totally clean 100% of the
time.  Absolutely all intermediate things needed during the build process will
go in the hashed junkyard. Another benefit is that nobs can use part of the
current state of the OS environment in naming the artifact (reflected in the
hash). For instance, you can already use the $OPTLEV and $DBGSYM environment
variables to control the `-O<level>` and `-g` compiler options and produce
different artifacts. Also, whatever you have in $CC and $CXX are used as the
compilers (defaulting to gcc/g++ otherwise).

```
export DBGSYM=1
export ASSERT=1
export OPTLEV=0
echo $(nobs exe test/hello_gasnet.cpp)
# prints: <upcxx>/.nobs/art/6c2d0a503e095800dfac643fd169e5f95a1260de.x

export DBGSYM=0
export ASSERT=0
export OPTLEV=2
echo $(nobs exe test/hello_gasnet.cpp)
# prints: <upcxx>/.nobs/art/833cfeddce3c6fa1b266ff9429f1202933639346.x

export DBGSYM=0
export ASSERT=0
export OPTLEV=3
echo $(nobs exe test/hello_gasnet.cpp)
# prints: <upcxx>/.nobs/art/58af363d4b6bcfa05e4768bf4c1f1e64d4e1e2ba.x

# Equivalently, thanks to succinct bash syntax:

DBGSYM=1 ASSERT=1 OPTLEV=0 echo $(nobs test/hello_gasnet.cpp)
DBGSYM=0 ASSERT=0 OPTLEV=2 echo $(nobs test/hello_gasnet.cpp)
DBGSYM=0 ASSERT=0 OPTLEV=3 echo $(nobs test/hello_gasnet.cpp)
```

Each of these will rebuild GASNet and the source file the first time they are
executed. After that, all three versions are in the cache and print
immediately. This allows you to switch between code-gen options as easily as
updating your environment. The output of `<c++> --version` is also used as a
caching dependency so if you switch programming environments (say using NERSC
`module swap`) nobs will automatically return the *right* artifact built against
the current toolchain. Oh, and obviously nobs watches the contents of the source
files being built, so if those change you'll also get a recompile. Artifacts
corresponding to previous versions of source files are implicitly removed from
the database. This is to prevent endless growth of the database when in a
development phase consisting of hack-compile-hack-compile-etc. The new artifacts
will have different hashes and the old files will be gone.

For whatever reason, if you ever think nobs got its hashes confused or is
missing some environmental dependency that ought to incur recompile, just `rm -r
.nobs` to nuke the database. That will ensure your next build will be
fresh. Then tell me!

If you want to dig in to how the build rules in nobs are specified, checkout
"nobsrule.py". I don't envision much reason to go hacking in there beyond
enhancing it w.r.t to its obvious deficiencies stated previously.

Enjoy.

## Compilers ##

The compiler choice in `nobs` is dictated by the following list in order of
decreasing precedence:

  1. Cross-compilation setting.
  2. User specified `CC` and `CXX` environment variables.
  3. `cc` and `CC` when running on NERSC's Cori or Edison.
  4. `gcc` and `g++` otherwise.

## External GASNet ##

Setting the `GASNET` environment variable to point to the absolute path of a
configured and built, or installed, GASNet-EX directory will cause `nobs` to skip
building its own. It is your responsibility that the compilers found by the
compiler selection mechanism above are compatible with those used by the given
gasnet. It is an error to have both the `GASNET` and `CROSS` variables set
simultaneously.

## GASNet Conduit ##

The gasnet conduit will be pulled from this precedence list:

  1. `GASNET_CONDUIT` enviornment variable.
  2. Cross-compilation setting (`cray-aries-*` maps to `aries`).
  3. `smp` otherwise.

## Cross-Compilation at NERSC ##

Cross compilation is currently supported for Cori and Edison. Have the
`CROSS=cray-aries-slurm` environment variable set while invoking `nobs` to use
the configuration settings found in gasnet's `other/contrib` directory. This
will use the current module's compilers and set gasnet to the `aries`
conduit. Without the `CROSS` variable, things default to the usual compiler
defaults and the `smp` conduit.

The following should be sufficient to run `test/hello_upcxx.cpp` on the
Cori/Edison compute nodes:

```
export CROSS=cray-aries-slurm
srun -n 10 $(nobs exe test/hello_upcxx.cpp)
```

## Errors ##

Build errors will happen. `nobs` will present them in a `less` instance, since,
when dealing with really long C++ errors you'll likely just want to see the
first few (`less` will not be invoked if either stdout or stderr are being
captured) . Make sure to report the stderr output of nobs to the appropriate
developer (if its all C++ errors) or to jdbachan in the case of gasnet
configure/build errors or uncaught Python exceptions. For gasnet errors, gasnet
might tell you to refer to some generated temporary file (likely in `/tmp/...`)
which won't exist since `nobs` cleans up all of its temporaries before exiting.
To prevent `/tmp` cleanup, run `nobs` with `NOBS_DEBUG=1` set.


## Internal-Only Configuration Notes ##

This serves as the place to document all the build-time and run-time tunables
that aren't hardened enough to be part of the user-facing docs.

### Compile Time Environment Variables ###

These are honored by `./install --single` but not the regular (non-single)
installation (which overwrites them in a parameter sweep loop).

* `OPTLEV=[0|1|2|3]`: Sets this optimization level for code generation. This
  value is passed as `-O${OPTLEV}` to the compiler.

* `DBGSYM=[0|1]`: Enables debugging symbols in executable (`-g` flag presence).

* `ASSERT=[0|1]`: Enables function of `UPCXX_ASSERT`.

* `UPCXX_MPSC_QUEUE=[atomic|biglock]`: The implementation to use for multi-
  producer single-consumer queues in the runtime (`upcxx::detail::intru_queue`).
    * `atomic`: (default) Highest performance: one atomic exchange per enqueue.
    * `biglock`: Naive global-mutex protected linked list. Low performance, but
      least risk with respect to potential bugs in implementation.
