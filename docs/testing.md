# Testing

Before installing UPC++, a simple test can be run to validate that UPC++ is
working correctly, given the current configuration and hardware. This can be
done by executing:

```bash
./run-tests
```

By default, the tests are run on a single node using the UDP conduit. 
The default number of ranks is set to a reasonable value, more than 2 
and no more than 16 or the number of hardware threads on the system. To 
specify an alternative number of ranks, set the environment variable 
`RANKS`, e.g. to run with 16 ranks, the script can be run as:

```bash
RANKS=16 ./run-tests
```

An alternative conduit can also be used, by setting the `CONDUIT` environment
variable, e.g.

```bash
CONDUIT=smp ./run-tests
```

To see available conduits, run 

```bash
./run-tests -h
```

When running on the Cray XC, `CROSS=cray-aries-slurm` must be set.

```bash
CROSS=cray-aries-slurm ./run-tests
```

The compiler used for the test is chosen according to the following list, in
order of decreasing precedence:

1. Cross-compilation setting (e.g. `CROSS=cray-aries-slurm` on Cray XC).
2. User-specified `CC` and `CXX` environment variables.
3. `cc` and `CC` when running on Cray XC systems.
4. `gcc` and `g++`. 

It is also possible to set the optimization level (`-O<level>`) and debugging
(`-g`) with environment variables. For example, the following will enable
optimization and disable debugging builds:

```bash
DBGSYM=0 OPTLEV=3 ./run-tests
```

`DBGSYM` can be either 0 or 1, and `OPTLEV` can be 0, 1, 2 or 3, corresponding
to optimization levels. By default, debugging is on and the optimization level
is 0.

