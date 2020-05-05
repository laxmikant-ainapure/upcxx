#!/bin/bash

# This script is used for BitBucket pipelines CI

rm -Rf test-results # this is just for CI testing

set -e
set -x

time ./configure --prefix=/usr/local/upcxx $CONFIGURE_ARGS

MAKE="make -j8"

time $MAKE all

time $MAKE install 

time $MAKE test_install || touch .pipe-fail

time $MAKE tests NETWORKS='smp udp' || touch .pipe-fail      # compile tests

# Run smp tests
# Don't use parallel make for runners or we risk memory exhaustion
time make run-tests RANKS=4 NETWORKS=smp || touch .pipe-fail  

# Run udp tests to simulate distributed memory
export GASNET_SPAWNFN=L
export GASNET_SUPERNODE_MAXSIZE=2
time make run-tests RANKS=4 NETWORKS=udp || touch .pipe-fail  

test ! -f .pipe-fail # propagate delayed failure
exit $?
