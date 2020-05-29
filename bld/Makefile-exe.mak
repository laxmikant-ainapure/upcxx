# Makefile fragment for cached "make exe" behavior

export SHELL = $(UPCXX_BASH)

# Variables for more readable rules
self       = $(upcxx_src)/bld/Makefile-exe.mak
target     = $(UPCXX_EXE)
depfile    = $(basename $(UPCXX_EXE)).dep
dot_d      = $(basename $(UPCXX_EXE)).d
library    = $(UPCXX_DIR)/lib/libupcxx.a
upcxx_meta = $(UPCXX_DIR)/bin/upcxx-meta

# Sequenced build (depfile then target) is simplest way to handle dependency tracking
# The use of `-s` prevents "[long full path] is up to date." messages.
# Indirect build of $(dot_d) addresses issue #370
default: force
	@if [[ ! -f $(depfile) ]]; then                             \
	   do_build=1;                                              \
	 elif ! $(MAKE) -s -f $(self) $(dot_d); then                \
	   echo "WARNING: forced rebuild of broken $(dot_d)";       \
	   rm -f $(dot_d);                                          \
	   do_build=1;                                              \
	 else                                                       \
	   do_build=0;                                              \
	 fi;                                                        \
	 if (( $$do_build )); then                                  \
	   $(MAKE) -f $(self) $(dot_d) UPCXX_NO_DEPS=1 || exit $$?; \
	 fi;                                                        \
	 if [[ $(dot_d) -nt $(depfile) ]]; then \
	   cp $(dot_d) $(depfile) || exit $$?; \
	 fi
	@$(MAKE) -s -f $(self) $(target)

force:

include $(upcxx_src)/bld/compiler.mak

.PHONY: default force

ifeq ($(UPCXX_NO_DEPS),)
ifneq ($(wildcard $(depfile)),)
include $(depfile)
endif
endif

$(dot_d): $(SRC) $(library)
	@source $(upcxx_meta) SET; \
	 case '$(SRC)' in \
	 *.c) eval "$(call UPCXX_DEP_GEN,$$CC  $$CFLAGS   $$CPPFLAGS,$@,$(SRC),$(EXTRAFLAGS)) > $@";; \
	 *)   eval "$(call UPCXX_DEP_GEN,$$CXX $$CXXFLAGS $$CPPFLAGS,$@,$(SRC),$(EXTRAFLAGS)) > $@";; \
	 esac

$(target): $(SRC) $(library) $(depfile)
	@source $(upcxx_meta) SET; \
	 case '$(SRC)' in \
	 *.c) cmd="$$CC  $$CCFLAGS  $$CPPFLAGS $$LDFLAGS $(SRC) $$LIBS -o $(target) $(UPCXX_COLOR_CFLAGS)   $(EXTRAFLAGS)";; \
	 *)   cmd="$$CXX $$CXXFLAGS $$CPPFLAGS $$LDFLAGS $(SRC) $$LIBS -o $(target) $(UPCXX_COLOR_CXXFLAGS) $(EXTRAFLAGS)";; \
	 esac; \
	 echo "$$cmd"; eval "$$cmd"
