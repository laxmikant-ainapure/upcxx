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

UPCXX_THREADMODE ?= seq

ENV = env UPCXX_THREADMODE=$(UPCXX_THREADMODE)

CXX = $(shell $(ENV) $(UPCXX_INSTALL)/bin/upcxx-meta CXX)
PPFLAGS = $(shell $(ENV) $(UPCXX_INSTALL)/bin/upcxx-meta PPFLAGS)
LDFLAGS = $(shell $(ENV) $(UPCXX_INSTALL)/bin/upcxx-meta LDFLAGS)
LIBFLAGS = $(shell $(ENV) $(UPCXX_INSTALL)/bin/upcxx-meta LIBFLAGS)

PTHREAD_FLAGS = -pthread
OPENMP_FLAGS = -fopenmp

EXTRA_FLAGS = -std=c++11

# Programs to build, assuming each has a corresponding *.cpp file
EXAMPLES = \
  compute-pi \
  compute-pi-multi-examples \
  hello-world \
  non-contig-example \
  persona-example \
  view-histogram1 \
  view-histogram2 \
  view-matrix-tasks

all: $(EXAMPLES)

# The rule for building any example.
.SECONDEXPANSION:
$(EXAMPLES): $$@.cpp $(wildcard *.h) $(wildcard *.hpp)
	$(CXX) $@.cpp $(PPFLAGS) $(LDFLAGS) $(EXTRA_FLAGS) $(LIBFLAGS) -o $@

# Example-specific variable specializations.
persona-example: UPCXX_THREADMODE = par
persona-example: EXTRA_FLAGS += $(OPENMP_FLAGS)
view-matrix-tasks: EXTRA_FLAGS += $(PTHREAD_FLAGS)

clean:
	rm -f $(EXAMPLES)

.PHONY: clean all

