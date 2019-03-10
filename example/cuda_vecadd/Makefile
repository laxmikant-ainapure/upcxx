# To use this makefile, set the UPCXX_INSTALL variable to the upcxx install directory, e.g.
# make UPCXX_INSTALL=<myinstalldir> all
# or (for bash)
# export UPCXX_INSTALL=<myinstalldir>; make all
 
ifeq ($(wildcard $(UPCXX_INSTALL)/bin/upcxx-meta),)
$(error Please set UPCXX_INSTALL=/path/to/upcxx/install)
endif


UPCXX_CUDA_NVCC ?= nvcc
NVCC_FLAGS ?= -arch=sm_30

EXTRA_FLAGS ?= -g

TARGETS = cuda_vecadd
OBJS = kernels.o
all: $(TARGETS)
clean:
	rm -f $(TARGETS) $(OBJS)
.PHONY: all clean

cuda_vecadd: vecadd.cpp kernels.cu kernels.hpp
	$(UPCXX_CUDA_NVCC) -c $(NVCC_FLAGS) $(EXTRA_FLAGS) kernels.cu -o kernels.o
	$(UPCXX_INSTALL)/bin/upcxx $(EXTRA_FLAGS) kernels.o vecadd.cpp -o $@
