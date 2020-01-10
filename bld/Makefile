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
makefiles = $(top_builddir)/Makefile $(top_srcdir)/bld/Makefile.rules $(top_srcdir)/bld/Makefile

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
GASNET_VAR_CMD = MAKEFLAGS='$(filter-out d -d --debug=%,$(MAKEFLAGS))' $(MAKE) -C $(builddir)/$(GASNETDIR) echovar VARNAME=$(1)
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

# If given a GASNet build directory in --single mode, treat it as read-only
ifeq ($(strip $(GASNET_TYPE)),build)
gasnet-single: ; @: # empty rule
else
gasnet-single: do-gasnet-configure
	@$(MAKE) -C "$(GASNETDIR)/$(GASNET_CONDUIT)-conduit" $(GASNET_THREADMODE)
	@if test $(GASNET_CONDUIT) = udp; then $(MAKE) -C "$(GASNETDIR)/other/amudp" amudprun; fi
endif

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
## Indirect targets to expose parallelism
## Pattern rules don't work as desired for the dependencies.
##

.PHONY: do-gasnet-configure-opt do-gasnet-configure-debug
.PHONY: do-gasnet-all-opt       do-gasnet-all-debug
.PHONY: do-gasnet-install-opt   do-gasnet-install-debug
.PHONY: do-gasnet-clean-opt     do-gasnet-clean-debug

do-gasnet-configure-opt: force
	@$(MAKE) do-gasnet-configure GASNET_CODEMODE=opt
do-gasnet-configure-debug: force
	@$(MAKE) do-gasnet-configure GASNET_CODEMODE=debug
do-gasnet-all-opt: do-gasnet-configure-opt
	@$(MAKE) -C gasnet.opt all
do-gasnet-all-debug: do-gasnet-configure-debug
	@$(MAKE) -C gasnet.debug all
do-gasnet-install-opt: do-gasnet-all-opt
	@$(MAKE) -C gasnet.opt install prefix=$(prefix)/gasnet.opt
do-gasnet-install-debug: do-gasnet-all-debug
	@$(MAKE) -C gasnet.debug install prefix=$(prefix)/gasnet.debug
do-gasnet-clean-opt: force
	@if [[ -d gasnet.opt ]]; then $(MAKE) -C gasnet.opt clean; fi
do-gasnet-clean-debug: force
	@if [[ -d gasnet.debug ]]; then $(MAKE) -C gasnet.debug clean; fi

##
## Standard (not incremental) targets for GASNet (opt and debug)
##
.PHONY: gasnet-configure gasnet-all gasnet-install gasnet-clean gasnet-distclean

gasnet-configure: do-gasnet-configure-opt do-gasnet-configure-debug
gasnet-all:       do-gasnet-all-opt       do-gasnet-all-debug
gasnet-install:   do-gasnet-install-opt   do-gasnet-install-debug
gasnet-clean:     do-gasnet-clean-opt     do-gasnet-clean-debug

gasnet-distclean: force
	rm -Rf gasnet.opt gasnet.debug

##
## Targets for generated scripts
## Uses define/endef, GNU Make's nearest equivalent of a heredoc
##

# Body of a bottom-level upcxx-meta (build and install)
do-upcxx-meta: force
	@file="$(DESTDIR)$(DST)/bin/upcxx-meta";        \
	 mkdir -p "$(DESTDIR)$(DST)/bin" || exit $$?;   \
	 rm -f $$file || exit $$?;                      \
	 echo "$$upcxx_meta_body" > $$file || exit $$?; \
	 chmod 755 $$file
.PHONY: do-upcxx-meta
ifneq ($(GASNET_BUILD),)
GASNET_BUILD_OR_INSTALL = GASNET_BUILD='$(GASNET_BUILD)'
else
GASNET_BUILD_OR_INSTALL = GASNET_INSTALL='$(prefix)/$(GASNETDIR)'
endif
RUNTIME_LIBS = $(GASNET_LIBS)
ifeq ($(UPCXX_CUDA),1)
RUNTIME_LIBS += $(UPCXX_CUDA_LIBFLAGS)
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
LIBS='-L$(DST)/lib -lupcxx $(RUNTIME_LIBS)'
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

LIBUPCXX_DEFINES := -DUPCXX_ASSERT_ENABLED=$(ASSERT) -DUPCXX_BACKEND=1 -D$(UPCXX_MPSC_QUEUE)=1
ifeq ($(strip $(UPCXX_BACKEND)),gasnet_seq)
LIBUPCXX_DEFINES += -DUPCXX_BACKEND_GASNET_SEQ=1
else
LIBUPCXX_DEFINES += -DUPCXX_BACKEND_GASNET_PAR=1
endif
ifeq ($(UPCXX_CUDA),1)
LIBUPCXX_DEFINES += -DUPCXX_CUDA_ENABLED=1 $(UPCXX_CUDA_CPPFLAGS)
endif


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

# In developer builds, embed git-describe output in runtime.o
ifneq ($(wildcard $(top_srcdir)/.git),)
UPCXX_GIT_VERSION := $(shell git --git-dir="$(top_srcdir)/.git" describe --always 2>/dev/null | grep '^upcxx-20[0-9][0-9]\.[0-9]')
ifneq ($(strip $(UPCXX_GIT_VERSION)),)
upcxx_git_version_file = backend/gasnet/runtime.cpp # consumes UPCXX_GIT_VERSION
upcxx_git_version_obj = $(notdir $(basename $(upcxx_git_version_file))).o
$(basename $(upcxx_git_version_obj))_EXTRA_FLAGS = -DUPCXX_GIT_VERSION='$(UPCXX_GIT_VERSION)'
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
$(libupcxx_objs): $(makefiles)
libupcxx.a: $(libupcxx_objs)
ifneq ($(strip $(upcxx_git_version_obj)),)
	@rm -f $(UPCXX_DIR)/lib/$(upcxx_git_version_obj); \
	 $(MAKE) -C $(UPCXX_DIR)/lib -f "$(builddir)/Makefile" $(upcxx_git_version_obj)
endif
	ar rcs $@ $?
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

.PHONY: do-upcxx-single upcxx-single

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

do-upcxx-all: force
	@$(MAKE) do-gasnet-configure
	@targets='';                                                           \
	 for conduit in $(GASNET_CONDUITS); do                                 \
	   for backend in $(ALL_BACKENDS); do                                  \
	     targets="$$targets $(call UPCXXDIR_FN,$(ASSERT),$(OPTLVL),$(DBGSYM),$$backend,$$conduit)/libupcxx.a";\
	   done;                                                               \
	 done;                                                                 \
	 $(MAKE) $$targets
do-upcxx-install-one: force
	@mkdir -p "$(DESTDIR)$(DST)/lib" || exit $$?;                     \
	 src="$(call UPCXXDIR)";                                          \
	 cp $$src/lib/libupcxx.a "$(DESTDIR)$(DST)/lib/" || exit $$?;     \
	 for file in `cd $$src && find include/upcxx/ -name '*.hpp'`; do  \
           file=`echo $$file | tr -s /`;                                  \
	   if grep "$$file" $$src/lib/upcxx_headers.d >/dev/null; then    \
	     mkdir -p "$(DESTDIR)$(DST)/`dirname $$file`" || exit $$?;    \
	     cp $$src/$$file "$(DESTDIR)$(DST)/$$file" || exit $$?;       \
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
	         UPCXX_FRAGMENTS="$(DESTDIR)$(prefix)/gasnet.debug/include"
	@$(MAKE) do-upcxx-install-all \
	         ASSERT=0 OPTLVL=3 DBGSYM=0 UPCXX_CODEMODE=O3    \
	         UPCXX_FRAGMENTS="$(DESTDIR)$(prefix)/gasnet.opt/include"
upcxx-clean: force
	rm -Rf $(dir $(all_libupcxx))
upcxx-distclean: upcxx-clean

#
# Targets for `install-single.sh prefix`
#
do-install-single: force
	@$(MAKE) do-upcxx-install-one DST="$(prefix)" UPCXX_FRAGMENTS="$(GASNET_BUILD)"
	@cd "$(GASNET_BUILD)/$(GASNET_CONDUIT)-conduit" && \
	    cp libgasnet-$(GASNET_CONDUIT)-$(GASNET_THREADMODE).a $(prefix)/lib/
install-single: upcxx-single
	@$(MAKE) do-install-single GASNET_BUILD="$(builddir)/$(GASNETDIR)"

.PHONY: do-install-single install-single

##
## Top-level targets
##

all install clean distclean: force
	@$(MAKE) gasnet-$@
	@$(MAKE) upcxx-$@

##
## "make exe SRC='foo.cpp' [EXTRAFLAGS=-Dfoo=bar]"
## Generated full path to resulting executable on stdout
##
## Correctly supports absolute and relative paths
## PARAMS: ASSERT, OPTLVL, DBGSYM, UPCXX_BACKEND, GASNET_CONDUIT
## OPTIONAL PARAMS: EXTRAFLAGS
##

# Hash absolute path of $(SRC), plus any $(EXTRAFLAGS), to form executable name
# The library variant is encoded in $(UPCXXDIR)
SRCPATH = $(shell cd $(STARTDIR) && cd $(dir $(SRC)) && pwd)/$(notdir $(SRC))
UPCXX_EXE_CSUM = $(firstword $(shell $(UPCXX_CSUMCMD) <<< '$(SRCPATH) $(EXTRAFLAGS)'))
UPCXX_EXE_STEM = $(builddir)/$(UPCXXDIR)/exe/$(UPCXX_EXE_CSUM)
UPCXX_EXE      = $(UPCXX_EXE_STEM)$(call GASNET_VAR,EXESUFFIX)

do-exe: force
	@mkdir -p $(UPCXXDIR)/exe
	@$(MAKE) $(UPCXXDIR)/libupcxx.a
	@$(MAKE) -C "$(STARTDIR)" -f $(top_srcdir)/bld/Makefile-exe.mak \
		UPCXX_DIR='$(builddir)/$(UPCXXDIR)' UPCXX_EXE='$(UPCXX_EXE)'

# NOTE: only the final $(UPCXX_EXE) may go to stdout
exe: force
	@$(MAKE) do-exe >&2
	@echo -n '$(UPCXX_EXE)'

# Clean exe files for current library variant
## PARAMS: ASSERT, OPTLVL, DBGSYM, UPCXX_BACKEND, GASNET_CONDUIT
exe-clean: force; @rm -Rf $(UPCXXDIR)/exe

.PHONY: do-exe exe exe-clean

##
## "make run SRC='bar.cpp' [EXTRAFLAGS=-Dfoo=bar] [ARGS='arg1 arg2'] [RANKS=n]
##
## Correctly supports absolute and relative paths
## PARAMS: ASSERT, OPTLVL, DBGSYM, UPCXX_BACKEND, GASNET_CONDUIT
## OPTIONAL PARAMS: RANKS, ARGS, EXTRAFLAGS
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

upcxx-meta upcxx: force
	@$(MAKE) $(UPCXXDIR)/libupcxx.a >&2
	@echo -n '$(builddir)/$(UPCXXDIR)/bin/$@'

upcxx-run: force
	@$(MAKE) gasnet-single >&2
	@$(MAKE) DST='$(builddir)/$(UPCXXDIR)' do-upcxx-run >&2
	@echo -n '$(builddir)/$(UPCXXDIR)/bin/$@'

.PHONY: upcxx-meta upcxx-run upcxx
