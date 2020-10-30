# Scripts for Testing by UPC++ Maintainers

This directory contains a collection of scripts to automate configure, build
and test of a developer's git working directory on a variety of systems,
curated to provide coverage of the compilers and networks which we support.
These include systems to which members of the core UPC++ development team
have, or can obtain, access and an allocation of time (where necessary).

## The Catalog

+ `catalina-xcode`  
    CLaSS's macOS 10.15 VM  
    Newest-available Apple Xcode (11.x)
+ `el_capitan-xcode_old`  
    CLaSS's macOS 10.11 VM  
    Oldest-supported Apple Xcode (8.x)
+ `dirac-{clang,gcc,intel,pgi}{,_old}`  
    CLaSS's Linux/x86-64 cluster with InfiniBand  
    Newest-available and oldest-supported versions of four compiler families
+ `cori-cray{,_old}`  
    NERSC's Cray XC40  
    Newest-available and oldest-supported PrgEnv-cray  
+ `theta-llvm{,_old}`  
    ALCF's Cray XC40  
    Newest-available and oldest-supported PrgEnv-llvm  
+ `summit-pgi{,_old}`  
    OLCF's Summit ppc64le cluster with dual-rail InfiniBand  
    Newest-available and oldest-supported versions of PGI's compilers  
    Worth noting that PGI compilers for x86-64 and ppc64le have been
    seen to exhibit different bugs, and consequently have different
    oldest-supported versions.
+ `wombat-{clang,gcc}{,_old}`
    An ARM64/InfiniBand cluster at ORNL.
    Newest-available and oldest-supported versions of clang and gcc

These scripts should also be considered as templates to be adapted to other
systems, compilers, or conditions (such as known failures to be skipped).  For
instance, to test a compiler version other than the oldest or newest on one of
these systems typically only requires editing a `module load ...` command.

## Usage

One typically runs these scripts from their location within a git working
directory containing the code to be tested.  The `dev-tests` and
`dev-run-tests` make targets are used to compile and run tests as described in
[build-devel.md](../../docs/build-devel.md).  The test-selection logic excludes
untracked files in the search for `.cpp` files.  Consequently, new tests must
be committed to git if they are to be run.  However, there is no requirement
that tests (or any other files) be "clean" with respect to `git`, nor any
requirement that commits be pushed prior to testing.

By default, running one of these script will create a build directory, below
the current working directory, named for the script with a date/time stamp
suffix.  This default can be overridden using the `--dir=...` option (see
below).  Everything described below (configure, make, runs, etc.) will occur
within this build directory.  All output from the script is also recorded in
`logfile.txt` within the build directory.

Be aware that the hundreds of test executables built may require tens of GB on
some systems.  So, one should take care in selecting where to run.

These scripts accept the following optional arguments:

* `--upcxx=...`  
  The path to the UPC++ source directory.  
  The default is the grandparent directory of the script itself.

* `--gasnet=...`  
  If provided, will be prefixed with `--with-gasnet=` and passed to the
  `configure` script.  This can be a path to a GASNet-EX source directory or
  a URL of a GASNet-EX tarball.  If this option is not provided, then no
  `--with-gasnet=` options is passed to `configure` and its default behavior
  applies (download of the GASNet-stable tarball, unless modified).

* `--dir=...`  
  Provides a working directory, replacing the default constructed from the
  script name and a time/date stamp.

* `--stages-...`  
  A comma-delimited set of stages to perform, from the set `configure`,
  `build`, `tests` and `run`.  These are described in more detail below.
  Together with the use of `--dir-...` one can separate the stages, such
  as to execute only the `run` in a batch job.

The scripts execute the following steps, with the four named as stages being
conditional:

1.  Factored boilerplate  
    Common concerns, including argument processing and error handling

2.  Environment setup  
    This includes such things as `module load ...` commands to get the desired
    compilers in `$PATH`, and environment variable settings which influnce the
    `dev-*` make targets.  The latter include enabling OpenMP tests for
    compilers with the necessary support, and `TEST_EXCLUDE_*` settings to
    skip known failures not in `bld/tests.mak` (such as version-dependent
    ones).

3.  Configure (stage name `configure`)  
    This includes the system-appropriate arguments for networks and job
    launch, as well as `--with-cuda` where supported.

4.  Build (stage name `build`)  
    This is `make -jN all ...` for some system-dependent default `N` (more on
    that below), plus some `CI_*` variables converted to make variables.

5.  Compile tests (stage name `tests`)  
    This is `make -jN dev-tests`

6.  Run tests (stage name `run`)  
    This is `make dev-run-tests` with platform-specific logic to use
    batch-scheduled resources where necessary.  On all current systems the
    execution is "synchronous", meaning the script will stall until the
    job has completed execution, with output displayed in real time.  There
    is no need to manaully track a job id, poll for job completion, or hunt
    for output file(s).

On Theta, the Compile and Run steps are split into `opt` and `debug` sub-steps
due to queue policies which make running the entire suite of tests in one job
impractical.

If any of the steps 1...4 fail, the script terminates immediately and the build
directory (printed in the final lines of output) is left intact.  This is a
"normal" build directory in which one can proceed to debug any failures.  One
should consult the ENVIRONMENT section of the individual scripts for any
environment modules or other settings necessary to use the build directory.

If the script completes all of the enabled steps without failure, then the test
executables are removed but the build directory is left otherwise intact (no
`make clean`, for instance).

## Environment

* `CI_MAKE`  
  Used of `CI_MAKE` can override the default `make`, for instance if/when
  `gmake` is required.

* `CI_MAKE_PARALLEL`  
  As noted in the list of steps, the scripts include a default level of
  parallel make when building the runtime and the tests.  One can override this
  by setting, for instance, `CI_MAKE_PARALLEL=''` for no parallelism, or
  `CI_MAKE_PARALLEL='-j999'` to get yourself in trouble with the sysadmins :-).

* `CI_NETWORKS`  
  Used to override the per-target default set of networks to set.
  For instance `CI_NETWORKS="smp mpi"`.

* `CI_EXTRAFLAGS`, `CI_TESTS` and `CI_NO_TESTS`  
  Passed to the `make dev-tests` command without the `CI_` prefix.
  See [build-devel.md](../../docs/build-devel.md) for description of
  `TESTS` and `NO_TESTS`.

* `CI_DEV_CHECK`  
  If set to `0` these scripts will replace the default `dev-tests` and
  `dev-run-tests` make targets with `tests` and `run-tests`.  This reduces
  these scripts' behavioral equivalent from `make dev-check` to `make check`.

## Random notes

### Dirac compiler licenses

On Dirac, we have "single-seat" licenses for the Intel and PGI compilers.
This can potentially limit the throughput of concurrent users, and has been
seen to occasionally lead to licence-checkout timeouts which the compiler may
treat as a fatal error.  When this occurs during a configure probe, things can
go down-hill based on the result incorrect probe result.

This situation would seem to argue against attempting to test the latest and
oldest versions of either compiler concurrently.  However, the license manager
allows one user on a given host to run multiple instances of the compiler,
such as via `make -jN`.  The license manager is not aware of distinct ssh
connections, or even of which compiler versions(s) the user is running.  As a
result, one can safely test multiple version of the Intel or PGI compiler so
long as all runs with a given compiler family are executing on a *single*
host.

### S.E.P. - "Somebody else's problem(s)"

These scripts do not handle ssh into their respective systems.

These scripts do not perform any git commands, such as to checkout a specific
branch to be tested.

Both of those capabilities are candidates for layering above these scripts.
However, _these_ scripts are intended to "start" from the git working
directory on the target system, such as will be the case within a Jenkins or
GitLab CI environment.
