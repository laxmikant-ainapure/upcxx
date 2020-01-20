#
# GASNet-related configuration
#

# All conduits supported (officially or not) by UPC++
ALL_CONDUITS = smp udp mpi ibv aries

# Conduits for which testing should be skipped by default
UNOFFICIAL_CONDUITS = mpi

# Map UPCXX configuration variables to GASNET_CODEMODE
# PARAMS: OPTLVL, DBGSYM
ifeq ($(OPTLVL)$(DBGSYM),01)
GASNET_CODEMODE ?= debug
else
GASNET_CODEMODE ?= opt
endif

# Map UPCXX configuration variables to GASNET_THREADMODE
# PARAMS: UPCXX_BACKEND
GASNET_THREADMODE = $(UPCXX_BACKEND:gasnet_%=%)

# Make functions to extract values of GASNet Makefile variables
#   $(call GASNET_VAR_VAL,GASNET_BLDDIR,VARNAME) ->  VARNAME="VAL"
#   $(call GASNET_VAR,GASNET_BLDDIR,VARNAME)     ->  VAL
GASNET_VAR_CMD = MAKEFLAGS='$(filter-out d -d --debug=%,$(MAKEFLAGS))' $(MAKE) -C $(1) echovar VARNAME=$(2)
GASNET_VAR_VAL = $(shell $(call GASNET_VAR_CMD,$(1),$(2)))
GASNET_VAR     = $(shell $(call GASNET_VAR_CMD,$(1),$(2)) | cut -d\" -f2)
