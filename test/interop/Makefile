# To use this makefile, ensure that Berkeley UPC and UPC++ are both in the PATH,
# or alternatively set UPCC and UPCXX to point to the UPC/UPC++ compiler wrappers
#
# For documentation, see docs/upc-interop.md

SHELL = /bin/bash

UPCXX_GASNET_CONDUIT ?= smp
OPTLVL ?= -g

UPC_FLAGS=
UPCXX_FLAGS=

UPCC  ?= upcc
UPCXX ?= upcxx

UPCXX_THREADMODE ?= seq
ifeq ($(strip $(UPCXX_THREADMODE)),seq)
  UPC_THREAD_FLAGS = -nopthreads
else
  UPC_THREAD_FLAGS = -nopthreads -uses-threads
endif

UPCC_CMD  = $(shell echo $(UPCC))  $(OPTLVL) -network $(UPCXX_GASNET_CONDUIT) $(UPC_THREAD_FLAGS) $(UPC_FLAGS)
UPCXX_CMD = $(shell echo $(UPCXX)) $(OPTLVL) $(UPCXX_FLAGS)

# ===========================================

export UPCXX_GASNET_CONDUIT

TARGETS = main_upc main_upcxx

TEST_OBJS = test_upc.o test_upcxx.o

all: $(TARGETS)

version:
	@type -p $(UPCXX) ; exit 0
	-$(UPCXX_CMD) --version
	@type -p $(UPCC) ; exit 0
	-$(UPCC_CMD) --version	

.upc.o:
	$(UPCC_CMD) -c $<

.cpp.o:
	$(UPCXX_CMD) -c $<

main_upc: main_upc.o $(TEST_OBJS)
	$(UPCC_CMD) -link-with='$(UPCXX_CMD)' $^ -o $@

main_upcxx: main_upcxx.o $(TEST_OBJS)
	$(UPCC_CMD) -link-with='$(UPCXX_CMD)' -extern-main $^ -o $@

clean: force
	rm -f *.o $(TARGETS)

UPCXX_RUN ?= upcxx-run
TEST_NODES = 2
TEST_PROCS = 8
TEST_UPC_SZ = 64
TEST_UPCXX_SZ = 16

run: force $(TARGETS)
	@for exe in $(TARGETS) ; do \
	  set -x ; \
	  env GASNET_MAX_SEGSIZE=$(TEST_UPC_SZ)MB UPC_SHARED_HEAP_SIZE=$(TEST_UPC_SZ)MB UPCXX_SEGMENT_MB=$(TEST_UPCXX_SZ) \
	  $(UPCXX_RUN) -N $(TEST_NODES) -n $(TEST_PROCS) $$exe ; \
	 done

.PHONY: version force do_target

.SUFFIXES: .upc
	
