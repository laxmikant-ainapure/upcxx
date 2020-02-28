# To use this makefile, set the UPCXX_INSTALL variable to the upcxx install directory, e.g.
# make UPCXX_INSTALL=<myinstalldir> all
# or (for bash)
# export UPCXX_INSTALL=<myinstalldir>; make all
 
ifeq ($(UPCXX_INSTALL),)
$(warning UPCXX_INSTALL environment variable is not set, assuming upcxx is in the PATH)
UPCXX=upcxx
UPCXXRUN=upcxx-run
else
ifeq ($(wildcard $(UPCXX_INSTALL)/bin/upcxx),)
$(error Please set UPCXX_INSTALL=/path/to/upcxx/install)
else
UPCXX=$(UPCXX_INSTALL)/bin/upcxx
UPCXXRUN=$(UPCXX_INSTALL)/bin/upcxx-run
endif
endif

UPCXX_CUDA_NVCC ?= nvcc
NVCC_FLAGS ?= -arch=sm_30

CXXFLAGS ?= -g # optional command-line override

TARGETS = cuda_vecadd
OBJS = kernels.o
all: $(TARGETS)
clean:
	rm -f $(TARGETS) $(OBJS)
.PHONY: all clean

cuda_vecadd: vecadd.cpp kernels.cu kernels.hpp
	$(UPCXX_CUDA_NVCC) -c $(NVCC_FLAGS) $(CXXFLAGS) kernels.cu -o kernels.o
	$(UPCXX) $(CXXFLAGS) kernels.o vecadd.cpp -o $@
