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
# The minimum required work is done, not configuring or compiling
# more than requested.  Normal use of make dependencies ensure things
# are not rebuilt unless there have been source changes.
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
# Numerous GNU Make extensions are used.
##

# From configure:
top_srcdir   = $(upcxx_src)
top_builddir = $(upcxx_bld)

#defaults:
ASSERT ?= 1
OPTLVL ?= 0
DBGSYM ?= 1
UPCXX_BACKEND ?= gasnet_seq
GASNET_CONDUIT ?= smp

# Derived
builddir = $(top_builddir)/bld

# libupcxx configuration space
ALL_ASSERTS  = 0 1
ALL_OPTLVLS  = 0 3   # GASNet's DEBUG and NDEBUG, respectively.  Other values reserved for future use.
ALL_DBGSYMS  = 0 1
ALL_BACKENDS = gasnet_seq gasnet_par
ALL_CONDUITS = smp udp mpi ibv aries

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
#   $(call UPCXXDIR_FN,$(ASSERT),$(OPTLVL),$(DBGSYM),$(UPCXX_BACKEND),$(GASNET_CONDUIT))
# or similar.
#
UPCXXDIR_FN = upcxx.assert$(1).optlvl$(2).dbgsym$(3).$(4).$(5)

# UPCXXDIR_FN for current values of all params
UPCXXDIR = $(call UPCXXDIR_FN,$(ASSERT),$(OPTLVL),$(DBGSYM),$(UPCXX_BACKEND),$(GASNET_CONDUIT))

# Similar for GASNet build directory
GASNETDIR = gasnet.$(GASNET_CODEMODE)

#
# Extraction of GASNet Makefile variables (ones not present in .mak fragment)
# GASNET_VAR_VAL:  VAR="VAL"
# GASNET_VAR:      VAL
#
GASNET_VAR_CMD = $(MAKE) -C $(builddir)/$(GASNETDIR) echovar VARNAME=$(1)
GASNET_VAR_VAL = $(shell $(call GASNET_VAR_CMD,$(1)))
GASNET_VAR     = $(shell $(call GASNET_VAR_CMD,$(1)) | cut -d\" -f2)

# Some useful GASNet variables
ifneq ($(wildcard $(builddir)/$(GASNETDIR)),)
GASNET_CONDUITS   := $(call GASNET_VAR,CONDUITS)
GASNET_CC_FAMILY  := $(call GASNET_VAR,CC_FAMILY)
GASNET_CXX_FAMILY := $(call GASNET_VAR,CXX_FAMILY)
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
	@dir=$(GASNETDIR);                                                            \
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
	   export CC='$(CONFIG_CC)' CXX='$(CONFIG_CXX)';                              \
	   mkdir -p "$$dir" && cd "$$dir" && eval "$$cmd";                            \
	 fi

#
# On-demand compile of a single instance of libgasnet
# PARAMS: OPTLVL, DBGSYM, UPCXX_BACKEND, GASNET_CONDUIT, GASNET, CROSS
#
gasnet-single: do-gasnet-configure
	@$(MAKE) -C "$(GASNETDIR)/$(GASNET_CONDUIT)-conduit" $(GASNET_THREADMODE)
	@if test $(GASNET_CONDUIT) = udp; then $(MAKE) -C "$(GASNETDIR)/other/amudp" amudprun; fi

#
# On-demand compile of a single instance of libgasnet by full path
#
all_libgasnet = $(foreach codemode,   opt debug,             \
                $(foreach conduit,    $(ALL_CONDUITS),       \
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
## Targets for generated scripts
## Uses define/endef, GNU Make's nearest equivalent of a heredoc
##

# Body of a bottom-level upcxx-meta (build and install)
do-upcxx-meta: force
	@file="$(DST)/bin/upcxx-meta";                  \
	 mkdir -p "$(DST)/bin" || exit $$?;             \
	 rm -f $$file || exit $$?;                      \
	 echo "$$upcxx_meta_body" > $$file || exit $$?;  \
	 chmod 755 $$file
.PHONY: do-upcxx-meta
ifneq ($(GASNET_BUILD),)
GASNET_BUILD_OR_INSTALL = GASNET_BUILD='$(GASNET_BUILD)'
else
GASNET_BUILD_OR_INSTALL = GASNET_INSTALL='$(prefix)/$(GASNETDIR)'
endif
# NOTE: the upcxx-meta file format below is rigid due to automated parsing
# do NOT add or remove blank lines
define upcxx_meta_body
#!/bin/sh
CC='$(GASNET_CC)'
CPPFLAGS='$(LIBUPCXX_DEFINES) -I$(DST)/include $(GASNET_CXXCPPFLAGS)'
CXX='$(GASNET_CXX)'
CXXFLAGS='$(LIBUPCXX_STDCXX) $(GASNET_CXXFLAGS)'
GASNET_CONDUIT='$(GASNET_CONDUIT)'
$(GASNET_BUILD_OR_INSTALL)
LDFLAGS='$(GASNET_LDFLAGS)'
LIBS='-L$(DST)/lib -lupcxx $(GASNET_LIBS)'
PPFLAGS="$$CPPFLAGS"
LIBFLAGS="$$LIBS"

[ -n "$$1" ] && eval [ -n \"\$$$$1\" ] && eval echo \"\$$$$1\"
endef

# Body of a upcxx-run wrapper in a build dir
do-upcxx-run: force
	@file='$(DST)/bin/upcxx-run';                  \
	 mkdir -p "$(DST)/bin" || exit $$?;            \
	 rm -f "$$file" || exit $$?;                   \
	 echo "$$upcxx_run_body" > $$file || exit $$?; \
	 chmod 755 $$file
.PHONY: do-upcxx-run
define upcxx_run_body
#!/bin/bash
export GASNET_PREFIX='$(builddir)/$(GASNETDIR)'
exec '$(top_srcdir)/utils/upcxx-run' "$$@"
endef

# Body of a upcxx wrapper in a build dir
# TODO: remove (with warning) any conflicting network,
# threadmode or codemode options instead of overriding
do-upcxx-script: force
	@file='$(DST)/bin/upcxx';                         \
	 mkdir -p "$(DST)/bin" || exit $$?;               \
	 rm -f "$$file" || exit $$?;                      \
	 echo "$$upcxx_script_body" > $$file || exit $$?; \
	 chmod 755 $$file
.PHONY: do-upcxx-script
define upcxx_script_body
#!/bin/bash
export UPCXX_META='$(builddir)/$(UPCXXDIR)/bin/upcxx-meta'
source '$(top_srcdir)/utils/upcxx.sh' "$$@" \
   -network=$(GASNET_CONDUIT) \
   -codemode=$(GASNET_CODEMODE) \
   -threadmode=$(GASNET_THREADMODE)
endef

# Don't let these templates polute the environment except when needed
ifneq ($(DST),)
export upcxx_meta_body
export upcxx_run_body
export upcxx_script_body
endif

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


# NOTE: sufficiently old GNU Make lacks 'else if*'
ifneq ($(findstring -std=c++,$(GASNET_CXX)),)
LIBUPCXX_STDCXX :=
else
ifeq ($(GASNET_CXX_FAMILY),Intel)
LIBUPCXX_STDCXX := -std=c++14
else
LIBUPCXX_STDCXX := -std=c++11
endif
endif

LIBUPCXX_C_COMPILE   := $(GASNET_CC)  $(GASNET_CPPFLAGS)    $(GASNET_CFLAGS) \
                        $(LIBUPCXX_DEFINES) -I$(UPCXX_DIR)/include
LIBUPCXX_CXX_COMPILE := $(GASNET_CXX) $(GASNET_CXXCPPFLAGS) $(GASNET_CXXFLAGS) $(LIBUPCXX_STDCXX) \
                        $(LIBUPCXX_DEFINES) -I$(UPCXX_DIR)/include

#
# Dependency tracking:
# Use of a temporary file fixes race between include and partial generation
#

%.d: %.c   ; tmp=tmp$$$$; $(LIBUPCXX_C_COMPILE)   -E -M -MT '$*.o $@' $< -o $$tmp && mv $$tmp $@
%.d: %.cpp ; tmp=tmp$$$$; $(LIBUPCXX_CXX_COMPILE) -E -M -MT '$*.o $@' $< -o $$tmp && mv $$tmp $@
ifneq ($(wildcard *.d),)
include $(wildcard *.d)
endif

# Pattern rules with dependency file as extra prerequisite
%.o: %.c   %.d; $(LIBUPCXX_C_COMPILE)   $($*_EXTRA_FLAGS) -c $< -o $@
%.o: %.cpp %.d; $(LIBUPCXX_CXX_COMPILE) $($*_EXTRA_FLAGS) -c $< -o $@

# Pattern rules for first build when dependency file is missing
# Must be the second of these two rule pairs
%.o: %.c  ; $(MAKE) -f $(builddir)/Makefile $*.d $@
%.o: %.cpp; $(MAKE) -f $(builddir)/Makefile $*.d $@


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

#
# Build upcxx library from its sources, with dependency tracking
# Also populates a bin/ with scripts suited for use in build tree
#
VPATH = $(addprefix $(top_srcdir)/src/, $(sort $(dir $(libupcxx_sources))))
libupcxx_objs = $(addsuffix .o,$(notdir $(basename $(libupcxx_sources))))
libupcxx.a: $(libupcxx_objs); ar rcs $@ $?
do-libupcxx: force
	@$(MAKE) -C $(UPCXX_DIR)/lib -f "$(builddir)/Makefile" libupcxx.a upcxx_headers.d
	@$(MAKE) DST=$(UPCXX_DIR) do-upcxx-meta do-upcxx-run do-upcxx-script

#
# On-demand compile of a single instance of libupcxx and required libgasnet
# PARAMS: ASSERT, OPTLVL, DBGSYM, UPCXX_BACKEND, GASNET_CONDUIT, GASNET, CROSS
#
do-upcxx-single: force
	@dir=$(UPCXXDIR);                                     \
	 mkdir -p $$dir/lib && mkdir -p $$dir/include &&      \
	 rm -f $$dir/include/upcxx &&                         \
	 ln -s "$(top_srcdir)/src" $$dir/include/upcxx &&     \
	 $(MAKE) do-libupcxx UPCXX_DIR="$(builddir)/$$dir"
upcxx-single: gasnet-single
	@$(MAKE) do-upcxx-single UPCXX_FRAGMENTS="$(builddir)/$(GASNETDIR)"
upcxx-single-bld: force
	@$(MAKE) do-upcxx-single UPCXX_FRAGMENTS="$(GASNET)"

.PHONY: do-upcxx-single upcxx-single upcxx-single-bld

#
# On-demand compile of a single instance of libupcxx by fullpath
#
all_libupcxx = $(foreach assert,  $(ALL_ASSERTS),        \
               $(foreach optlvl,  $(ALL_OPTLVLS),        \
               $(foreach dbgsym,  $(ALL_DBGSYMS),        \
               $(foreach backend, $(ALL_BACKENDS),       \
               $(foreach conduit, $(ALL_CONDUITS),       \
	         $(call UPCXXDIR_FN,$(assert),$(optlvl),$(dbgsym),$(backend),$(conduit))/libupcxx.a)))))
$(all_libupcxx): force
	@tmp=`echo $@ | tr . /`; \
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
.PHONY: do-upcxx-all do-upcxx-install-one do-upcxx-install-all

#
# Helpers to iterate over conduits and backends
#

# Ugh.  From GNU Make 3.80 NEWS:
# * The arguments to $(call ...) functions were being stored in $1, $2,
#   etc. as recursive variables, even though they are fully expanded
#   before assignment.  This means that escaped dollar signs ($$ etc.)
#   were not behaving properly.  Now the arguments are stored as simple
#   variables.  This may mean that if you added extra escaping to your
#   $(call ...) function arguments you will need to undo it now.
ifneq ($(firstword $(sort $(MAKE_VERSION) 3.80)), 3.80)
# older than 3.80
do_upcxx_all_target = $(call UPCXXDIR_FN,$(ASSERT),$(OPTLVL),$(DBGSYM),$$$$backend,$$$$conduit)
else
do_upcxx_all_target = $(call UPCXXDIR_FN,$(ASSERT),$(OPTLVL),$(DBGSYM),$$backend,$$conduit)
endif

do-upcxx-all: force
	@$(MAKE) do-gasnet-configure
	@targets='';                                                           \
	 for conduit in $(GASNET_CONDUITS); do                                 \
	   for backend in $(ALL_BACKENDS); do                                  \
	     targets="$$targets $(do_upcxx_all_target)/libupcxx.a";\
	   done;                                                               \
	 done;                                                                 \
	 $(MAKE) $$targets
do-upcxx-install-one: force
	@mkdir -p "$(DST)/lib" || exit $$?;                               \
	 src="$(call UPCXXDIR)";                                          \
	 cp $$src/lib/libupcxx.a "$(DST)/lib/" || exit $$?;               \
	 for file in `cd $$src && find include/upcxx/ -name '*.hpp'`; do  \
           file=`echo $$file | tr -s /`;                                  \
	   if grep "$$file" $$src/lib/upcxx_headers.d >/dev/null; then    \
	     mkdir -p "$(DST)/`dirname $$file`" || exit $$?;              \
	     cp $$src/$$file "$(DST)/$$file" || exit $$?;                 \
	   fi;                                                            \
	 done
	@$(MAKE) do-upcxx-meta
do-upcxx-install-all: force
	@for conduit in $(GASNET_CONDUITS); do                               \
	   for backend in $(ALL_BACKENDS); do                                \
	     $(MAKE) do-upcxx-install-one GASNET_BUILD=''                    \
	       DST="$(prefix)/upcxx.$(UPCXX_CODEMODE).$$backend.$$conduit"   \
	       UPCXX_BACKEND=$$backend GASNET_CONDUIT=$$conduit || exit $$?; \
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
	@$(MAKE) do-upcxx-install-all \
	         ASSERT=1 OPTLVL=0 DBGSYM=1 UPCXX_CODEMODE=debug \
	         UPCXX_FRAGMENTS="$(prefix)/gasnet.debug/include"
	@$(MAKE) do-upcxx-install-all \
	         ASSERT=0 OPTLVL=3 DBGSYM=0 UPCXX_CODEMODE=O3    \
	         UPCXX_FRAGMENTS="$(prefix)/gasnet.opt/include"
upcxx-clean: force
	rm -Rf $(dir $(all_libupcxx))
upcxx-distclean: upcxx-clean

#
# Targets for `install --single prefix`
#
do-install-single: force
	@$(MAKE) do-upcxx-install-one DST="$(prefix)" UPCXX_FRAGMENTS="$(GASNET_BUILD)"
	@cd "$(GASNET_BUILD)/$(GASNET_CONDUIT)-conduit" && \
	    cp libgasnet-$(GASNET_CONDUIT)-$(GASNET_THREADMODE).a $(prefix)/lib/
install-single-src: upcxx-single
	@$(MAKE) do-install-single GASNET_BUILD="$(builddir)/$(GASNETDIR)"
install-single-bld: upcxx-single-bld
	@$(MAKE) do-install-single GASNET_BUILD="$(GASNET)"

.PHONY: do-install-single install-single-src install-single-bld

##
## Top-level targets
##

all install clean distclean: force
	@$(MAKE) gasnet-$@
	@$(MAKE) upcxx-$@

##
## "make exe SRC='foo.cpp src/bar.cpp'"
## Generated full path to resulting executable on stdout
##
## Correctly supports multiple source files and relative paths
## PARAMS: ASSERT, OPTLVL, DBGSYM, UPCXX_BACKEND, GASNET_CONDUIT
##

# Hash absolute paths of all filenames in $(SRC) to form executable name
# The library variant is encoded in $(UPCXXDIR)
ABSPATH = $(shell cd $(STARTDIR) && cd $(dir $(1)) && pwd)/$(notdir $(1))
UPCXX_CSUM = $(firstword $(shell $(UPCXX_CSUMCMD) <<< $(foreach src,$(SRC),$(call ABSPATH,$(src)))))
UPCXX_EXE  = $(builddir)/$(UPCXXDIR)/exe/$(UPCXX_CSUM).x

do-exe: force
	@mkdir -p $(UPCXXDIR)/exe
	@$(MAKE) $(UPCXXDIR)/libupcxx.a
	@cd "$(STARTDIR)" || exit $$?;                      \
	 source $(builddir)/$(UPCXXDIR)/bin/upcxx-meta SET; \
	 eval "$$CXX $$EXTRAFLAGS $$CXXFLAGS $$CPPFLAGS $$LDFLAGS $(SRC) $$LIBS -o $(UPCXX_EXE)"

# NOTE: only the final $(UPCXX_EXE) may go to stdout
exe: force
	@$(MAKE) do-exe >&2
	@echo -n '$(UPCXX_EXE)'

# Clean exe files for current library variant
## PARAMS: ASSERT, OPTLVL, DBGSYM, UPCXX_BACKEND, GASNET_CONDUIT
exe-clean: force; @rm -Rf $(UPCXXDIR)/exe

.PHONY: do-exe exe exe-clean

##
## "make run SRC='bar.cpp' ARGS='arg1 arg2' RANKS=n
##
## Correctly supports multiple source files and relative paths
## PARAMS: ASSERT, OPTLVL, DBGSYM, UPCXX_BACKEND, GASNET_CONDUIT
## OPTIONAL PARAMS: RANKS, ARGS
##
RANKS ?= 4

run: force
	@$(MAKE) do-exe
	@export GASNET_PREFIX=$(builddir)/$(GASNETDIR); \
	 eval $(top_srcdir)/utils/upcxx-run -n $(RANKS) $(UPCXX_EXE) $(ARGS)

.PHONY: run

##
## Wrappers
## Generates full path to requested wrapper on stdout
##
## PARAMS: ASSERT, OPTLVL, DBGSYM, UPCXX_BACKEND, GASNET_CONDUIT
##

upcxx-meta upcxx-run upcxx: force
	@$(MAKE) $(UPCXXDIR)/libupcxx.a >&2
	@echo -n '$(builddir)/$(UPCXXDIR)/bin/$@'

.PHONY: upcxx-meta upcxx-run upcxx
