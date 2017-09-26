# Testing

Before installing UPC++, a simple test can be run to validate that UPC++ is working correctly, given
the current configuration and hardware. This can be done by executing:

```bash
./run-tests
```

This will run a set of basic tests to ensure UPC++ is working correctly. By default, the tests are
run on a single node using the SMP conduit. The default number of ranks is the number of hardware
threads on the system. To specify an alternative number of ranks, set the environment variable
`RANKS`, e.g. to run with 16 ranks, the script can be run as:

```bash
RANKS=16 ./run-tests
```

The compiler used for the test is chosen according to the following list, in order of decreasing
precedence:

1. Cross-compilation setting (e.g. `CROSS=cray-aries-slurm` on Cray XC).
2. User-specified `CC` and `CXX` environment variables.
3. `cc` and `CC` when running on Cray XC systems.
4. `gcc` and `g++`. 

The conduit used for communication can also be changed from the SMP default, using the
`GASNET_CONDUIT` environment variable, e.g. to set the conduit to use UDP:

```bash
export GASNET_CONDUIT=udp
```
It is also possible to set the optimization level (`-O<level>`) and debugging (`-g`) with environment
variables. For example, the following will disable optimization and enable debugging builds:

```bash
DBGSYM=1 OPTLEV=0 ./run-tests
```

`DBGSYM` can be either 0 or 1, and `OPTLEV` can be 0, 1, 2 or 3, corresponding to optimization levels.

