##
# THIS IS A WORK-IN-PROGRESS
#
# Proof-of-concept for deferred/incremental building of libraries.
# To build a single libupcxx and its gasnet conduit from the top-level
# upcxx source directory:
#   $ make -C bld upcxx-single
#
# Additionally, targets "all", "install", "clean" and "distclean" are
# provided.  The "install" is still lacking `bin` and `share/doc`.
#
# All of the following env vars are considered at each invocation:
#   ASSERT, OPTLVL, DBGSYM, UPCXX_BACKEND, GASNET_CONDUIT
# For libgasnet:
#   The minimum required work is done, not configuring or compiling
#   more than requested.  Normal use of make dependencies ensure things
#   are not rebuilt if there have been no GASNet source changes.
# For libupcxx:
#   The entire library is rebuilt every time due to lack of proper
#   dependencies.  Adoption of automake or cmake should resolve that.
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
# At the moment numerous GNU Make extensions are used, and it is unclear if
# use of automake or cmake can eliminate them all.
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
ifeq ($(strip $(UPCXX_BACKEND)),gasnet_seq)
GASNET_THREADMODE = seq
else
GASNET_THREADMODE = par
endif

#
# Encode upcxx build directory name.  Used via
#   $(call UPCXXDIR,$(ASSERT),$(OPTLVL),$(DBGSYM),$(UPCXX_BACKEND),$(GASNET_CONDUIT))
# or similar.
#
UPCXXDIR = upcxx.assert$(1).optlvl$(2).dbgsym$(3).$(4).$(5)


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

ifneq ($(UPCXX_FRAGMENTS),)
include $(UPCXX_FRAGMENTS)/$(GASNET_CONDUIT)-conduit/$(GASNET_CONDUIT)-$(GASNET_THREADMODE).mak
endif

LIBUPCXX_DEFINES := -DUPCXX_ASSERT_ENABLED=$(ASSERT) -DUPCXX_BACKEND=1
ifeq ($(strip $(UPCXX_BACKEND)),gasnet_seq)
LIBUPCXX_DEFINES += -DUPCXX_BACKEND_GASNET_SEQ=1
else
LIBUPCXX_DEFINES += -DUPCXX_BACKEND_GASNET_PAR=1
endif
ifeq ($(UPCXX_CUDA),1)
LIBUPCXX_DEFINES += -DUPCXX_CUDA_ENABLED=1
endif
# TODO: magic missing here for DUPCXX_MPSC_QUEUE_*
LIBUPCXX_DEFINES += -DUPCXX_MPSC_QUEUE_ATOMIC=1

ifeq ($(findstring -std=c++,$(GASNET_CXX)),)
LIBUPCXX_STDCXX := -std=c++11
else
LIBUPCXX_STDCXX :=
endif

LIBUPCXX_C_COMPILE   := $(GASNET_CC)  $(GASNET_CPPFLAGS)    $(GASNET_CFLAGS) \
                        $(LIBUPCXX_DEFINES) -I$(UPCXX_DIR)/include
LIBUPCXX_CXX_COMPILE := $(GASNET_CXX) $(GASNET_CXXCPPFLAGS) $(GASNET_CXXFLAGS) $(LIBUPCXX_STDCXX) \
                        $(LIBUPCXX_DEFINES) -I$(UPCXX_DIR)/include

libupcxx_sources = \
	backend/gasnet/noise_log.cpp \
	backend/gasnet/runtime.cpp   \
	backend/gasnet/upc_link.c    \
	future/core.cpp              \
	atomic.cpp                   \
	barrier.cpp                  \
	broadcast.cpp                \
	copy.cpp                     \
	cuda.cpp                     \
	diagnostic.cpp               \
	digest.cpp                   \
	global_fnptr.cpp             \
	os_env.cpp                   \
	persona.cpp                  \
	reduce.cpp                   \
	rget.cpp                     \
	rput.cpp                     \
	segment_allocator.cpp        \
	serialization.cpp            \
	team.cpp                     \
	upcxx.cpp                    \
	vis.cpp                      \
	dl_malloc.c

# TODO: generate from libupcxx_sources
libupcxx_objs = $(UPCXX_DIR)/lib/*.o

# TODO: dependencies, objext, etc using automake or cmake
# Lack of dependences here is the biggest flaw
do-libupcxx: force
	@for file in $(libupcxx_sources); do                   \
	   case $$file in                                      \
	    *.cpp) stem=`basename $$file .cpp`                 \
	           cmd="$(LIBUPCXX_CXX_COMPILE)";;             \
	    *.c)   stem=`basename $$file .c`                   \
	           cmd="$(LIBUPCXX_C_COMPILE)";;               \
	    *)     echo unknown source type $$file; exit 1;;   \
	   esac;                                               \
	   cmd="$$cmd -c \"$(top_srcdir)/src/$$file\" -o $(UPCXX_DIR)/lib/$$stem.o"; \
	   echo "$$cmd"; eval "$$cmd" || exit $$?;             \
	 done
	ar rcs $(UPCXX_DIR)/lib/libupcxx.a $(libupcxx_objs)
	ranlib $(UPCXX_DIR)/lib/libupcxx.a

#
# On-demand compile of a single instance of libupcxx and required libgasnet
# PARAMS: ASSERT, OPTLVL, DBGSYM, UPCXX_BACKEND, GASNET_CONDUIT, GASNET, CROSS
#
upcxx-single: gasnet-single force
	@dir=$(call UPCXXDIR,$(ASSERT),$(OPTLVL),$(DBGSYM),$(UPCXX_BACKEND),$(GASNET_CONDUIT)); \
	 mkdir -p $$dir/lib && mkdir -p $$dir/include && \
	 ln -sf "$(top_srcdir)/src" $$dir/include/upcxx && \
	 $(MAKE) do-libupcxx UPCXX_DIR="$(builddir)/$$dir" \
	                     UPCXX_FRAGMENTS="$(builddir)/gasnet.$(GASNET_CODEMODE)"


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
	         $(call UPCXXDIR,$(assert),$(optlvl),$(dbgsym),$(backend),$(conduit))/libupcxx.a)))))
$(all_libupcxx): force
	 tmp=`echo $@ | tr . /`; \
	 assert=`expr "$$tmp" : '.*/assert\([0-1]\)/'`; \
	 optlvl=`expr "$$tmp" : '.*/optlvl\([0-3]\)/'`; \
	 dbgsym=`expr "$$tmp" : '.*/dbgsym\([0-1]\)/'`; \
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

# These could be just `...` to avoid GNU Make extension
conduit_list = $(shell grep GASNETI_CONDUITS gasnet.$(GASNET_CODEMODE)/gasnet_config.h | cut -d\" -f2)
header_list = $(shell cd "$(top_srcdir)/src" && find . -name '*.hpp')

# TODO: header_list contains some internal headers we don't need to install

do-upcxx-all: force
	@$(MAKE) do-gasnet-configure
	@for conduit in $(conduit_list); do \
	   for backend in gasnet_seq gasnet_par; do \
             $(MAKE) upcxx-single UPCXX_BACKEND=$$backend GASNET_CONDUIT=$$conduit || exit $$?; \
	   done; \
	 done
do-upcxx-meta: force
	@dst="$(prefix)/upcxx.$(UPCXX_CODEMODE).$(UPCXX_BACKEND).$(GASNET_CONDUIT)";\
	 meta="$$dst/bin/upcxx-meta";                                  \
	 rm -f $$meta || exit $$?;                                     \
	 (                                                             \
	   echo "#!/bin/sh";                                           \
	   echo "CC='$(GASNET_CC)'";                                   \
	   echo "CPPFLAGS='$(LIBUPCXX_DEFINES) -I$$dst/include $(GASNET_CXXCPPFLAGS)'";\
	   echo "CXX='$(GASNET_CXX)'";                                 \
	   echo "CXXFLAGS='$(LIBUPCXX_STDCXX) $(GASNET_CXXFLAGS)'";    \
	   echo "GASNET_CONDUIT='$(GASNET_CONDUIT)'";                  \
	   echo "GASNET_INSTALL='$(prefix)/gasnet.$(GASNET_CODEMODE)'";\
	   echo "LDFLAGS='$(GASNET_LDFLAGS)'";                         \
	   echo "LIBS='-L$$dst/lib -lupcxx $(GASNET_LIBS)'";           \
	   echo 'PPFLAGS="$$CPPFLAGS"';                                \
	   echo 'LIBFLAGS="$$LIBS"';                                   \
	   echo;                                                       \
	   echo '[ "$$1" != "" ] && eval echo '"'\$$'"'"$$1"';         \
	 ) > $$meta || exit $$?;                                       \
	 chmod 755 $$meta
do-upcxx-install: force
	@for conduit in $(conduit_list); do \
	   for backend in gasnet_seq gasnet_par; do                          \
	     src="$(call UPCXXDIR,$(ASSERT),$(OPTLVL),$(DBGSYM),$$backend,$$conduit)"; \
	     dst="$(prefix)/upcxx.$(UPCXX_CODEMODE).$$backend.$$conduit";    \
	     mkdir -p "$$dst/lib" || exit $$?;                               \
	     cp $$src/lib/libupcxx.a "$$dst/lib/" || exit $$?;               \
	     for header in $(header_list); do                                \
	       dir="$$dst/include/upcxx/`dirname $$header`";                 \
	       mkdir -p "$$dir" || exit $$?;                                 \
	       cp $$src/include/upcxx/$$header "$$dir/" || exit $$?;         \
	     done;                                                           \
	     mkdir -p "$$dst/bin" || exit $$?;                               \
	     $(MAKE) do-upcxx-meta UPCXX_BACKEND=$$backend                   \
	                           GASNET_CONDUIT=$$conduit || exit $$?;     \
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
	@$(MAKE) do-upcxx-install ASSERT=1 OPTLVL=0 DBGSYM=1 UPCXX_CODEMODE=debug \
	                          UPCXX_FRAGMENTS="$(prefix)/gasnet.debug/include"
	@$(MAKE) do-upcxx-install ASSERT=0 OPTLVL=3 DBGSYM=0 UPCXX_CODEMODE=O3    \
	                          UPCXX_FRAGMENTS="$(prefix)/gasnet.opt/include"
	@echo "TODO: install top-level bin and share directories"
upcxx-clean: force
	rm -Rf $(dir $(all_libupcxx))
upcxx-distclean: upcxx-clean

##
## Top-level targets for testing
##

all install clean distclean: force
	@$(MAKE) gasnet-$@
	@$(MAKE) upcxx-$@
