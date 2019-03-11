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
CXX = $(UPCXX_INSTALL)/bin/upcxx

PTHREAD_FLAGS = -pthread
OPENMP_FLAGS = -fopenmp

EXTRA_FLAGS = -g

# Programs to build, assuming each has a corresponding *.cpp file
EXAMPLES = \
  dmap-test \
  dmap-async-insert-test \
  dmap-async-find-test \
  dmap-quiescence-test \
  dmap-atomics-test \
  dmap-conjoined-test \
  dmap-promises-test \
  rb1d-av \
  hello-world \
  view-histogram1 \
  view-histogram2 \
  view-matrix-tasks \
  non-contig-example \
  persona-example-rputs \
  persona-example	


CUDA_EXAMPLES = \
  h-d \
  h-d-remote

all: $(EXAMPLES)
cuda: $(CUDA_EXAMPLES)

# The rule for building any example.
%: %.cpp $(wildcard *.h) $(wildcard *.hpp)
	$(CXX) $@.cpp $(EXTRA_FLAGS) -o $@


# Example-specific variable specializations.
persona-example: export UPCXX_THREADMODE=par
persona-example: EXTRA_FLAGS += $(PTHREAD_FLAGS)
persona-example-rputs: export UPCXX_THREADMODE=par
persona-example-rputs: EXTRA_FLAGS += $(PTHREAD_FLAGS)
view-matrix-tasks: EXTRA_FLAGS += $(PTHREAD_FLAGS)

clean:
	rm -f $(EXAMPLES)
	rm -f $(CUDA_EXAMPLES)

.PHONY: clean all

