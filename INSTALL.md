
To install, from the main directory run:

```bash
./install <installdirname>
```
This will install UPC++ to the `installdirname` directory. Note that this install downloads the
GASNet communication library, so an Internet connection is needed.

If you are installing on Cray XC, before running install, further setup is needed:

```bash
export CROSS=cray-aries-slurm
export GASNET_CONDUIT=aries
module switch PrgEnv-intel PrgEnv-gnu
```
On a Mac, make sure you install the Xcode Command Line Tools with the following command line:

```bash
xcode-select --install
```

If you have set up any other non-clang compilers in your path, be sure
to adjust your path so that g++ and gcc resolve to /usr/bin.
If there are any issues with the installation, you can clean it by running `rm -r .nobs`.

To compile, use the `${UPCXX_INSTALL}/bin/upcxx-meta` helper script, where `UPCXX_INSTALL` is the
installation directory. For example, to build the hello world code given previously, execute:

```bash
g++ --std=c++11 hello-world.cpp -o hello-world $($UPCXX_INSTALL/bin/upcxx-meta PPFLAGS) \
    $($UPCXX_INSTALL/bin/upcxx-meta LDFLAGS) $($UPCXX_INSTALL/bin/upcxx-meta LIBFLAGS)
```

On Cray systems, `CC` should be used instead of `g++`. However, note that we do not currently support
Intel compilers (hence the `module switch` command listed earlier).

The compiled code can be run directly on an SMP node, but it will only run with one rank unless you
set the number of ranks at run time. With the default SMP conduit (on a single node), this can be
done with the `GASNET_PSHM_NODES` environment variable. So, for example, to run the hello world code
on 8 ranks, use:

```bash
GASNET_PSHM_NODES=8 ./hello-world
```

For Cray XC with Slurm, the command for running hello world with 8 ranks would be:

```bash
srun -n 8  ./hello-world
```

For an example of a Makefile for building UPC++ applications, look at
`example/prog-guide/Makefile`. This directory also has code for running all the examples given in
the guide. To use that `Makefile`, first set the `UPCXX_INSTALL` shell variable to the install
path.

