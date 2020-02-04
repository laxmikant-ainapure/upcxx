#!/bin/bash

set -e
function cleanup { rm -f conftest.c; }
trap cleanup EXIT

cat >conftest.c <<_EOF
#include <gasnetex.h>
#ifndef GASNET_HIDDEN_AM_CONCURRENCY_LEVEL
  RESULT=-%UNDEF%-=
#elif GASNET_HIDDEN_AM_CONCURRENCY_LEVEL
  RESULT=-%1%-=
#else
  RESULT=-%0%-=
#endif
_EOF

[[ $(eval ${GASNET_CC} ${GASNET_CPPFLAGS} ${GASNET_CFLAGS} -E conftest.c) =~ RESULT=-%(.+)%-= ]]
result="${BASH_REMATCH[1]}"
case "$result" in
  0|1)   echo "#define UPCXX_HIDDEN_AM_CONCURRENCY_LEVEL $result";;
  UNDEF) echo "#undef UPCXX_HIDDEN_AM_CONCURRENCY_LEVEL";;
  *)     echo "ERROR probing GASNET_HIDDEN_AM_CONCURRENCY_LEVEL" >&2; exit 1;;
esac
