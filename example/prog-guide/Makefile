# To use this makefile, make sure the UPCXX variable is set to the upcxx install directory, e.g.
# make UPCXX_INSTALL=<myinstalldir> hello-world
# or
# export UPCXX_INSTALL=<myinstalldir>; make hello-world

ifeq ($(wildcard $(UPCXX_INSTALL)/bin/upcxx-meta),)
$(error Please set UPCXX_INSTALL=/path/to/upcxx/install)
endif

EXTRA_FLAGS=-std=c++11 -Wno-inline
CXX=$(shell $(UPCXX_INSTALL)/bin/upcxx-meta CXX)
PPFLAGS=$(shell $(UPCXX_INSTALL)/bin/upcxx-meta PPFLAGS) $(EXTRA_FLAGS)
LDFLAGS=$(shell $(UPCXX_INSTALL)/bin/upcxx-meta LDFLAGS) $(EXTRA_FLAGS)
LIBFLAGS=$(shell $(UPCXX_INSTALL)/bin/upcxx-meta LIBFLAGS)
OPENMP_FLAGS=-fopenmp

all: hello-world compute-pi compute-pi-multi-examples persona-example

CP_HDRS=async-distobj-reduce_to_rank0.hpp distobj-reduce_to_rank0.hpp global-ptrs-reduce_to_rank0.hpp \
	rpc-reduce_to_rank0.hpp atomics-reduce_to_rank0.hpp fetch.hpp quiescence-reduce_to_rank0.hpp \
	rpc-reduce_to_rank0-no-barrier.hpp

compute-pi-multi-examples: compute-pi-multi-examples.cpp $(CP_HDRS)
	$(CXX) $< $(PPFLAGS) $(LDFLAGS) $(LIBFLAGS) -o $@ 

OPENMP_EXAMPLES = persona-example
$(OPENMP_EXAMPLES): % : %.cpp
	@if test -z "$$UPCXX_BUILDING_PAR" ; then \
	  export UPCXX_BUILDING_PAR=1 ; \
	  export UPCXX_THREADMODE="par" ; \
	  $(MAKE) $@ EXTRA_FLAGS="$(EXTRA_FLAGS) $(OPENMP_FLAGS)" ; \
	else set -x ; \
	  $(CXX) $< $(PPFLAGS) $(LDFLAGS) $(LIBFLAGS) -o $@ ; \
	fi

.cpp: *.h *.hpp
	$(CXX) $< $(PPFLAGS) $(LDFLAGS) $(LIBFLAGS) -o $@

clean:
	rm -rf hello-world compute-pi compute-pi-multi-examples persona-example

.PHONY: force

