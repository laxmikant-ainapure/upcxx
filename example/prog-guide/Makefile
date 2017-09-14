# To use this makefile, make sure the UPCXX variable is set to the upcxx install directory, e.g.
# make UPCXX_INSTALL=<myinstalldir> hello-world
# or
# export UPCXX_INSTALL=<myinstalldir>; make hello-world

ifeq ($(wildcard $(UPCXX_INSTALL)/bin/upcxx-meta),)
$(error Please set UPCXX_INSTALL=/path/to/upcxx/install)
endif

PPFLAGS=$(shell $(UPCXX_INSTALL)/bin/upcxx-meta PPFLAGS)
LDFLAGS=$(shell $(UPCXX_INSTALL)/bin/upcxx-meta LDFLAGS)
LIBFLAGS=$(shell $(UPCXX_INSTALL)/bin/upcxx-meta LIBFLAGS)

CXX := $(CXX) -std=c++11

all: hello-world compute-pi compute-pi-multi-examples

CP_HDRS=async-distobj-accumulate.hpp distobj-accumulate.hpp global-ptrs-accumulate.hpp \
	rpc-accumulate.hpp atomics-accumulate.hpp fetch.hpp quiesence-accumulate.hpp

compute-pi-multi-examples: compute-pi-multi-examples.cpp $(CP_HDRS)
	$(CXX) $< $(PPFLAGS) $(LDFLAGS) $(LIBFLAGS) -o $@ 

.cpp: *.h *.hpp
	$(CXX) $< $(PPFLAGS) $(LDFLAGS) $(LIBFLAGS) -o $@

clean:
	rm -rf hello-world compute-pi compute-pi-multi-examples

