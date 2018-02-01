# This Makefile demonstrates the recommended way to build simple UPC++ programs.
# Note this uses some GNU make extensions for conciseness.
#
# To use this makefile, set the UPCXX_INSTALL variable to the upcxx install directory, e.g.
# make UPCXX_INSTALL=<myinstalldir> hello-world
# or (for bash)
# export UPCXX_INSTALL=<myinstalldir>; make hello-world

ifeq ($(wildcard $(UPCXX_INSTALL)/bin/upcxx-meta),)
$(error Please set UPCXX_INSTALL=/path/to/upcxx/install)
endif

EXTRA_FLAGS=-std=c++11
CXX=$(shell $(UPCXX_INSTALL)/bin/upcxx-meta CXX)
PPFLAGS=$(shell $(UPCXX_INSTALL)/bin/upcxx-meta PPFLAGS)
LDFLAGS=$(shell $(UPCXX_INSTALL)/bin/upcxx-meta LDFLAGS)
LIBFLAGS=$(shell $(UPCXX_INSTALL)/bin/upcxx-meta LIBFLAGS)

# Programs to build, assuming each has a corresponding *.cpp file
EXAMPLES = hello-world compute-pi compute-pi-multi-examples persona-example

all: $(EXAMPLES)

.cpp: *.h *.hpp
	$(CXX) $< $(PPFLAGS) $(LDFLAGS) $(EXTRA_FLAGS) $(LIBFLAGS) -o $@

# This part shows a specialized rule for building an OpenMP example
OPENMP_EXAMPLES = persona-example
OPENMP_FLAGS=-fopenmp
$(OPENMP_EXAMPLES): % : %.cpp
	@if test -z "$$UPCXX_BUILDING_PAR" ; then \
	  export UPCXX_BUILDING_PAR=1 ; \
	  export UPCXX_THREADMODE="par" ; \
	  $(MAKE) $@ EXTRA_FLAGS="$(EXTRA_FLAGS) $(OPENMP_FLAGS)" ; \
	else set -x ; \
	  $(CXX) $< $(PPFLAGS) $(LDFLAGS) $(EXTRA_FLAGS) $(LIBFLAGS) -o $@ ; \
	fi

clean:
	rm -f $(EXAMPLES)

.PHONY: clean all

