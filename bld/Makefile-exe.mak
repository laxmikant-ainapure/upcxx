# Makefile fragment for cached "make exe" behavior

# Variables for more readable rules
self       = $(upcxx_src)/bld/Makefile-exe.mak
target     = $(UPCXX_EXE)
depfile    = $(basename $(UPCXX_EXE)).d
library    = $(UPCXX_DIR)/lib/libupcxx.a
upcxx_meta = $(UPCXX_DIR)/bin/upcxx-meta

# Sequenced build (depfile then target) is simplest way to handle dependency tracking
# The use of `-s` prevents "[long full path] is up to date." messages.
default: force
	@$(MAKE) -s -f $(self) $(depfile)
	@$(MAKE) -s -f $(self) $(target)

force:

.PHONY: default force

ifneq ($(wildcard $(depfile)),)
include $(depfile)
endif

$(depfile): $(SRC) $(library)
	@source $(upcxx_meta) SET; \
	 case '$(SRC)' in \
	 *.c) eval "$$CC  $$CFLAGS   $$CPPFLAGS -E -M -MT '$(target) $(depfile)' $(SRC) -o $(depfile) $(EXTRAFLAGS)";; \
	 * )  eval "$$CXX $$CXXFLAGS $$CPPFLAGS -E -M -MT '$(target) $(depfile)' $(SRC) -o $(depfile) $(EXTRAFLAGS)";; \
	 esac

$(target): $(SRC) $(library) $(depfile)
	@source $(upcxx_meta) SET; \
	 case '$(SRC)' in \
	 *.c) cmd="$$CC  $$CCFLAGS  $$CPPFLAGS $$LDFLAGS $(SRC) $$LIBS -o $(target) $(EXTRAFLAGS)";; \
	 *)   cmd="$$CXX $$CXXFLAGS $$CPPFLAGS $$LDFLAGS $(SRC) $$LIBS -o $(target) $(EXTRAFLAGS)";; \
	 esac; \
	 echo "$$cmd"; eval "$$cmd"
