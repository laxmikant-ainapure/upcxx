#!/bin/bash

set -e
function cleanup { rm -f conftest.txt; }
trap cleanup EXIT

env CONFIG_COMMANDS= CONFIG_LINKS= CONFIG_HEADERS= \
    CONFIG_FILES=conftest.txt:${UPCXX_TOPSRC}/bld/gasnet_config.mak.in \
    ./config.status >/dev/null
cat conftest.txt
