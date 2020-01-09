#!/bin/bash

if [ "$#" -eq 1 ]; then
  prefix="$1"
else
  echo usage: $(basename $0) INSTALL_DIR >&2
  exit 1
fi

# Configure:
./configure --prefix="${prefix}" --single || exit $?

# Extract:
GMAKE=$(grep '^export GMAKE=' Makefile | cut -d= -f2) || exit $?

# Install:
${UPCXX_MAKE:-$GMAKE -j8} install-single
if [[ $? -ne 0 ]]; then
  echo 'UPC++ install failed'
  exit 1;
fi
echo 'UPC++ successfully installed'
