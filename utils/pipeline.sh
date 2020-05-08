#!/bin/bash

# This script is used for BitBucket pipelines CI

# Optional Inputs: (shell or pipeline variables)
#  CI_CONFIGURE_ARGS : extra arguments for the configure line
#  CI_MAKE_PARALLEL : argument to set build parallelism, eg "-j4"
#  CI_DEV_CHECK : "1" to enable extra dev-mode tests
#  CI_NETWORKS : override default network "smp udp"
#  CI_RANKS : override default rank count

echo "----------------------------------------------"
echo "CI info:"
echo "Repo       "$BITBUCKET_REPO_FULL_NAME ;
echo "Branch     "$BITBUCKET_BRANCH ;
echo "Tag        "$BITBUCKET_TAG ;
echo "Commit     "$BITBUCKET_COMMIT ;
echo "PR         "$BITBUCKET_PR_ID ;
echo "----------------------------------------------"
echo "Inputs:"
echo "CI_CONFIGURE_ARGS=$CI_CONFIGURE_ARGS"
echo "CI_DEV_CHECK=$CI_DEV_CHECK"
CI_MAKE_PARALLEL=${CI_MAKE_PARALLEL:--j8}
echo "CI_MAKE_PARALLEL=$CI_MAKE_PARALLEL"
CI_NETWORKS="${CI_NETWORKS:-smp udp}"
echo "CI_NETWORKS=$CI_NETWORKS"
CI_RANKS="${CI_RANKS:-4}"
echo "CI_RANKS=$CI_RANKS"
echo "----------------------------------------------"

MAKE="make $CI_MAKE_PARALLEL"
if (( "$CI_DEV_CHECK" )) ; then
  DEV="dev-"
else
  DEV=""
fi

set -e
set -x

time ./configure --prefix=/usr/local/upcxx $CI_CONFIGURE_ARGS

time $MAKE all

time $MAKE install 

time $MAKE test_install || touch .pipe-fail

time $MAKE ${DEV}tests NETWORKS="$CI_NETWORKS" || touch .pipe-fail      # compile tests

# Run smp tests
if expr " $CI_NETWORKS " : " smp " ; then
  # Don't use parallel make for runners or we risk memory exhaustion
  time make ${DEV}run-tests RANKS=$CI_RANKS NETWORKS=smp || touch .pipe-fail  
fi

# Run udp tests to simulate distributed memory
if expr " $CI_NETWORKS " : " udp " ; then
  export GASNET_SPAWNFN=L
  export GASNET_SUPERNODE_MAXSIZE=${GASNET_SUPERNODE_MAXSIZE:-2}
  time make ${DEV}run-tests RANKS=$CI_RANKS NETWORKS=udp || touch .pipe-fail  
fi

test ! -f .pipe-fail # propagate delayed failure
exit $?
