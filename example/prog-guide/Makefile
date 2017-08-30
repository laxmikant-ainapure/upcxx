# To use this makefile, make sure the UPCXX variable is set to the upcxx install directory, e.g.
# make UPCXX_INSTALL=<myinstalldir> hello-world
# or
# export UPCXX_INSTALL=<myinstalldir>; make hello-world

PPFLAGS=$(shell $(UPCXX_INSTALL)/bin/upcxx-meta PPFLAGS)
LDFLAGS=$(shell $(UPCXX_INSTALL)/bin/upcxx-meta LDFLAGS)
LIBFLAGS=$(shell $(UPCXX_INSTALL)/bin/upcxx-meta LIBFLAGS)

all: hello-world compute-pi compute-pi-multi-examples

hello-world: hello-world.cpp
	$(CXX) $< $(PPFLAGS) $(LDFLAGS) $(LIBFLAGS) -o $@ 

compute-pi: compute-pi.cpp
	$(CXX) $< $(PPFLAGS) $(LDFLAGS) $(LIBFLAGS) -o $@ 

CP_HDRS=async-distobj-accumulate.hpp distobj-accumulate.hpp global-ptrs-accumulate.hpp \
	rpc-accumulate.hpp atomics-accumulate.hpp fetch.hpp quiesence-accumulate.hpp

compute-pi-multi-examples: compute-pi-multi-examples.cpp $(CP_HDRS)
	$(CXX) $< $(PPFLAGS) $(LDFLAGS) $(LIBFLAGS) -o $@ 

clean:
	rm -rf hello-world compute-pi compute-pi-multi-examples

