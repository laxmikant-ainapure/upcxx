# To use this makefile, make sure the UPCXX variable is set to the upcxx install directory, e.g.
# make UPCXX_INSTALL=<myinstalldir> hello-world
# or
# export UPCXX_INSTALL=<myinstalldir>; make hello-world

ifeq ($(wildcard $(UPCXX_INSTALL)/bin/upcxx-meta),)
$(error Please set UPCXX_INSTALL=/path/to/upcxx/install)
endif

CXX=$(shell $(UPCXX_INSTALL)/bin/upcxx-meta CXX) -std=c++11
PPFLAGS=$(shell $(UPCXX_INSTALL)/bin/upcxx-meta PPFLAGS) -Wno-inline
LDFLAGS=$(shell $(UPCXX_INSTALL)/bin/upcxx-meta LDFLAGS) -Wno-inline
LIBFLAGS=$(shell $(UPCXX_INSTALL)/bin/upcxx-meta LIBFLAGS)

all: hello-world compute-pi compute-pi-multi-examples

CP_HDRS=async-distobj-reduce_to_rank0.hpp distobj-reduce_to_rank0.hpp global-ptrs-reduce_to_rank0.hpp \
	rpc-reduce_to_rank0.hpp atomics-reduce_to_rank0.hpp fetch.hpp quiescence-reduce_to_rank0.hpp \
	rpc-reduce_to_rank0-no-barrier.hpp

compute-pi-multi-examples: compute-pi-multi-examples.cpp $(CP_HDRS)
	$(CXX) $< $(PPFLAGS) $(LDFLAGS) $(LIBFLAGS) -o $@ 

.cpp: *.h *.hpp
	$(CXX) $< $(PPFLAGS) $(LDFLAGS) $(LIBFLAGS) -o $@

clean:
	rm -rf hello-world compute-pi compute-pi-multi-examples

