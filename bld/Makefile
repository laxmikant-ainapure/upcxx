##
# THIS IS A WORK-IN-PROGRESS
#
# Proof-of-concept for deferred/incremental building of GASNet conduits.
# To build a single conduit from the top-level upcxx source.
#   $ make -C bld gasnet-single
#
# All of the following env vars are considered at each invocation:
#   OPTLVL, DBGSYM, UPCXX_BACKEND, GASNET_CONDUIT
# The minimum required work is done, not configuring or compiling
# more than requested.
# Normal use of make dependencies ensure things are not rebuilt if
# there have been no GASNet source changes.
#
# The following env vars are considered only at first use:
#   GASNET, CROSS
# and thus these should NOT change over the live of a given build.
# Additionally, the only supported value for GASNET is a source tree.
# However, these restrictions would be natural if these two are specified
# to configure or CMake (where a URL-valued GASNET would result in fetch
# and unpack).
#
# Currently there is no handling of CC or CXX.
# Again the assumption is that these are fixed for the given build directory.
#
# The assumptions that GASNET, CROSS, CC, and CXX are fixed when one runs
# configure or cmake seem reasonable, since both tools support VPATH builds
# such that multiple build dirs can be used per (clean) source dir.  However,
# that is not implemented yet.
#
# At the moment GNU Make extensions are used, including:
# + .PHONY is a "safety net" and its presence/absence doesn't impact correctness
# + Uses of 'ifeq' can be replaced with (repeated) shell code or other tricks
# + Unsure about a uniform portable replacement '?='. However, this is only used
#   now to give the environment precedence over the defaults - something that
#   is unnecessary if the (non-human?) caller sets these on the command line.
##

# Examples: autoconf/automake would replace:
top_srcdir   = $(upcxx_src)
top_builddir = $(upcxx_src)
prefix       = NO DEFAULT
GASNET      ?= NO DEFAULT
MAKE        := make

#defaults:
ASSERT ?= 1
OPTLVL ?= 0
DBGSYM ?= 1
UPCXX_BACKEND ?= gasnet_seq
GASNET_CONDUIT ?= smp

#
# Determine the GASNET_CODEMODE
# Currently debug *only* for OPTLVL=0 DBGSYM=1
# PARAMS: OPTLVL, DBGSYM
#
ifeq ($(OPTLVL)$(DBGSYM),01)
GASNET_CODEMODE ?= debug
else
GASNET_CODEMODE ?= opt
endif

#
# Determine the GASNET_THREADMODE
# PARAMS: UPCXX_BACKEND
#
ifeq ($(UPCXX_BACKEND),gasnet_seq)
GASNET_THREADMODE = seq
else
GASNET_THREADMODE = par
endif

dummy:
	@echo NO DEFAULT TARGET
force:

.PHONY: dummy force


##
## Targets for lazy/incremental execution of GASNet configure and compile steps
##
.PHONY: do-gasnet-configure gasnet-single

#
# On-demand configure of a GASNet build directory (opt or debug)
# PARAMS: OPTLVL, DBGSYM, GASNET, CROSS
#
configure_opt=--disable-debug
configure_debug=--enable-debug
do-gasnet-configure: force
	@dir=gasnet.$(GASNET_CODEMODE);                                               \
	 if test \! -f "$$dir/gasnet_config.h" ; then                                 \
	   echo "Configuring $(GASNET_CODEMODE)-mode build of GASNet";                \
	   if test -n "$(CROSS)"; then                                                \
	     cmd="SRCDIR=$(GASNET) $(GASNET)/other/contrib/cross-configure-$(CROSS)"; \
	   else                                                                       \
	     cmd="$(GASNET)/configure";                                               \
	   fi;                                                                        \
	   cmd="$$cmd $$GASNET_CONFIGURE_ARGS";                                       \
	   cmd="$$cmd --disable-parsync --enable-seq --enable-par";                   \
	   cmd="$$cmd --enable-pthreads --disable-segment-everything";                \
	   cmd="$$cmd --enable-$(GASNET_CONDUIT) $(configure_$(GASNET_CODEMODE))";    \
	   echo "$$cmd";                                                              \
	   mkdir -p "$$dir" && cd "$$dir" && eval "$$cmd";                            \
	 fi

#
# On-demand compile of a single instance of libgasnet
# PARAMS: OPTLVL, DBGSYM, UPCXX_BACKEND, GASNET_CONDUIT, GASNET, CROSS
#
gasnet-single: do-gasnet-configure
	 $(MAKE) -C "gasnet.$(GASNET_CODEMODE)/$(GASNET_CONDUIT)-conduit" $(GASNET_THREADMODE)

#
# On-demand compile of a single instance of libgasnet by full path
#
# NOTE: $(all_libgasnet) or the equivalent multiple rules could be generated
# by automake or cmake instead of using this GNU Make syntax
all_libgasnet = $(foreach codemode,   opt debug,             \
                $(foreach conduit,    smp udp mpi ibv aries, \
                $(foreach threadmode, seq par,               \
                  gasnet.$(codemode)/$(conduit)-conduit/libgasnet-$(conduit)-$(threadmode).a)))
$(all_libgasnet): force
	@tmp=`echo $@ | tr .- //`;              \
	 codemode=`echo $$tmp | cut -d/ -f2`;   \
	 conduit=`echo $$tmp | cut -d/ -f3`;    \
	 threadmode=`echo $$tmp | cut -d/ -f7`; \
	 $(MAKE) gasnet-single GASNET_CODEMODE=$$codemode GASNET_CONDUIT=$$conduit GASNET_THREADMODE=$$threadmode

##
## Standard (not incremental) targets for GASNet (opt and debug)
##
.PHONY: gasnet-configure gasnet-all gasnet-install gasnet-clean gasnet-distclean

gasnet-configure: force
	@$(MAKE) do-gasnet-configure GASNET_CODEMODE=opt
	@$(MAKE) do-gasnet-configure GASNET_CODEMODE=debug
gasnet-all: gasnet-configure
	@$(MAKE) -C gasnet.opt   all
	@$(MAKE) -C gasnet.debug all
gasnet-install: gasnet-all
	@$(MAKE) -C gasnet.opt   install prefix=$(prefix)/gasnet.opt
	@$(MAKE) -C gasnet.debug install prefix=$(prefix)/gasnet.debug
gasnet-clean: force
	@if test -d gasnet.opt  ; then $(MAKE) -C gasnet.opt   clean; fi
	@if test -d gasnet.debug; then $(MAKE) -C gasnet.debug clean; fi
gasnet-distclean: force
	rm -Rf gasnet.opt gasnet.debug

##
## Targets for incremental builds of libupcxx
##

#
# On-demand compile of a single instance of libupcxx and required libgasnet
# PARAMS: ASSERT, OPTLVL, DBGSYM, UPCXX_BACKEND, GASNET_CONDUIT, GASNET, CROSS
#
upcxx-single: force
	@$(MAKE) gasnet-single
	@target=upcxx.$(ASSERT).$(OPTLVL).$(DBGSYM).$(UPCXX_BACKEND).$(GASNET_CONDUIT)/libupcxx.a; \
	 echo "TODO: no rule(s) yet to build $$target"

#
# On-demand compile of a single instance of libupcxx by fullpath
#
# As noted w/ all_gasnet, use of GNU Make's "foreach" is not required
# if/when using automake or cmake
all_libupcxx = $(foreach assert,  0 1,                   \
               $(foreach optlvl,  0 1 2 3,               \
               $(foreach dbgsym,  0 1,                   \
               $(foreach backend, gasnet_seq gasnet_par, \
               $(foreach conduit, smp udp mpi ibv aries, \
	         upcxx.$(assert).$(optlvl).$(dbgsym).$(backend).$(conduit)/libupcxx.a)))))
$(all_libupcxx): force
	@tmp=`echo $@ | tr . /`; \
	 assert=`echo $$tmp | cut -d/ -f2`;  \
	 optlvl=`echo $$tmp | cut -d/ -f3`;  \
	 dbgsym=`echo $$tmp | cut -d/ -f4`;  \
	 backend=`echo $$tmp | cut -d/ -f5`; \
	 conduit=`echo $$tmp | cut -d/ -f6`; \
	 $(MAKE) upcxx-single ASSERT=$$assert OPTLVL=$$optlvl DBGSYM=$$dbgsym \
                              UPCXX_BACKEND=$$backend GASNET_CONDUIT=$$conduit

##
## Standard (not incremental) targets for upcxx install
## Note that these just cover UPCXX_CODEMODE of O3 or debug
##
.PHONY: upcxx-all upcxx-install upcxx-clean upcxx-distclean

#
# Helpers to iterate over conduits and backends
#

# This be just `...` to avoid GNU Make extension
conduit_list = $(shell grep GASNETI_CONDUITS gasnet.$(GASNET_CODEMODE)/gasnet_config.h | cut -d\" -f2)

do-upcxx-all: force
	@$(MAKE) do-gasnet-configure
	@for conduit in $(conduit_list); do \
	   for backend in gasnet_seq gasnet_par; do \
             $(MAKE) upcxx-single UPCXX_BACKEND=$$backend GASNET_CONDUIT=$$conduit || exit $$?; \
	   done; \
	 done
do-upcxx-install: force
	@for conduit in $(conduit_list); do \
	   for backend in gasnet_seq gasnet_par; do                          \
	     src="upcxx.$(ASSERT).$(OPTLVL).$(DBGSYM).$$backend.$$conduit";  \
	     dst="upcxx.$(UPCXX_CODEMODE).$$backend.$$conduit";              \
	     mkdir -p "$(prefix)/$dst" || exit $$?;                          \
	     echo "TODO: install from $$src to $(prefix)/$$dst";             \
	   done; \
	 done

#
# Standard all, install, clean and distclean
# There is no configure
#
upcxx-all: force
	@$(MAKE) do-upcxx-all ASSERT=1 OPTLVL=0 DBGSYM=1
	@$(MAKE) do-upcxx-all ASSERT=0 OPTLVL=3 DBGSYM=0
upcxx-install: upcxx-all
	@$(MAKE) do-upcxx-install ASSERT=1 OPTLVL=0 DBGSYM=1 UPCXX_CODEMODE=debug
	@$(MAKE) do-upcxx-install ASSERT=0 OPTLVL=3 DBGSYM=0 UPCXX_CODEMODE=opt
	@echo "TODO: automake or cmake can/should install bin and share directories"
upcxx-clean: force
	rm -Rf $(dir $(all_libupcxx))
upcxx-distclean: upcxx-clean

##
## Top-level targets for testing
##

all install clean distclean: force
	@$(MAKE) gasnet-$@
	@$(MAKE) upcxx-$@
