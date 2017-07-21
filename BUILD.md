# Building UPC\+\+ #

Requirements:

  - Bash
  - Python 2.x >= 2.6
  - C++11 compiler: (gcc >= 4.9) or (clang >= ???) or (icc >= ???)

## Immediate Deficiencies ##

The build system is good enough for our near-term needs. Current
deficiencies which will need to be resolved before release:

  1. Can only build fully linked executables. Need a way to produce
     upcxx library artifact and associated metadata.
  2. Internally manages its own GASNet builds. Need to support
     externally supplied GASNet.
  3. Only GASNet conduit supported is SMP, need to extend to high-perf
     conduits of our target supercomputers. Requires using the
     "contrib/" configure scripts for some.

## Workflow ##

The first thing to do when beginning a session of upcxx hacking is to
source the `sourceme` bash script. This will populate your current
bash environment with the needed commands for developing and building
upcxx.

```
cd <upcxx>
. sourceme
```

The buildsystem "nobs" is now availabe under the `nobs` bash function,
but only for this bash instance. First useful thing to try:

```
nobs exe test/gasnet_hello.cpp
```

This should download gasnet, build it, then build and link 
"test/gasnet_hello.cpp" and eventually write some cryptic hash 
encrusted filename to stdout. That is the path to our just built 
executable. At the moment gasnet defaults to building in the SMP 
conduit, so to test out a parallel run do this:

```
GASNET_PSHM_NODES=2 $(nobs exe test/gasnet_hello.cpp)
```

Or equivalently this convenience command which implicitly runs the
executable after building (comes in handy when you want nice paging
of error messages in the case of build failure, which nobs does really
well):

```
GASNET_PSHM_NODES=2 nobs run test/gasnet_hello.cpp
```

All object files and executables and all other artifacts of the build 
process are stored internally in "upcxx/.nobs/art". Their names are 
cryptic hashes so don't bother looking around. To get the name  of
something you care about just use bash code like
`mything=$(nobs ...)`. The big benefit to managing our artifacts this
way is that we can keep our source tree totally clean 100% of the time.
Absolutely all intermediate things needed during the build process will
go in the hashed junkyard. Another benefit is that nobs can use part
of the  current state of the OS environment in naming the artifact
(reflected in the hash). For instance, you can already use the $OPTLEV
and $DBGSYM environment variables to control the `-O<level>` and `-g`
compiler options and produce different artifacts. Also, whatever you
have in $CC and $CXX are used as the compilers (defaulting to gcc/g++
otherwise).

```
export DBGSYM=1
export OPTLEV=0
echo $(nobs exe test/gasnet_hello.cpp)
# prints: <upcxx>/.nobs/art/6c2d0a503e095800dfac643fd169e5f95a1260de.x

export DBGSYM=0
export OPTLEV=2
echo $(nobs exe test/gasnet_hello.cpp)
# prints: <upcxx>/.nobs/art/833cfeddce3c6fa1b266ff9429f1202933639346.x

export DBGSYM=0
export OPTLEV=3
echo $(nobs exe test/gasnet_hello.cpp)
# prints: <upcxx>/.nobs/art/58af363d4b6bcfa05e4768bf4c1f1e64d4e1e2ba.x

# Equivalently, thanks to succinct bash syntax:

DBGSYM=1 OPTLEV=0 echo $(nobs test/gasnet_hello.cpp)
DBGSYM=0 OPTLEV=2 echo $(nobs test/gasnet_hello.cpp)
DBGSYM=0 OPTLEV=3 echo $(nobs test/gasnet_hello.cpp)
```

Each of these will rebuild GASNet and the source file the first time 
they are executed. After that, all three versions are in the cache and 
print immediately. This allows you to switch between code-gen options 
as easily as updating your environment. The output of `<c++> --version` 
is also used as a caching dependency so if you switch programming 
environments (say using NERSC `module swap`) nobs will automatically 
return the *right* artifact built against the current toolchain. Oh, 
and obviously nobs watches the contents of the source files being 
built, so if those change you'll also get a recompile. Artifacts 
corresponding to previous versions of source files are implicitly 
removed from the database. This is to prevent endless growth of the 
database when in a development phase consisting of 
hack-compile-hack-compile-etc. The new artifacts will have different 
hashes and the old files will be gone.

For whatever reason, if you ever think nobs got its hashes confused or 
is missing some environmental dependency that ought to incur recompile, 
just `rm -r .nobs` to nuke the database. That will ensure your next
build will be fresh. Then tell me!

If you want to dig in to how the build rules in nobs are specified,
checkout "nobsrule.py". I don't envision much reason to go hacking in
there beyond enhancing it w.r.t to its obvious deficiencies stated
previously.

Enjoy.
