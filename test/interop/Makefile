# To use this makefile, ensure that Berkeley UPC and UPC++ are both in the PATH,
# or alternatively set UPCC and UPCXX to point to the UPC/UPC++ compiler wrappers
#
# For documentation, see docs/upc-hybrid.md

SHELL = /bin/bash

# optional settings overridable from environment or command-line
UPCXX_NETWORK ?= smp
UPCXX_THREADMODE ?= seq

OPTLVL ?= -g
CFLAGS ?= 
CXXFLAGS ?= 

# ----------------------------------------------------------------
ifeq ($(UPCXX_INSTALL)$(UPCXX),)
  $(warning UPCXX_INSTALL environment variable is not set, assuming upcxx is in the PATH)
  UPCXX = upcxx
else
  UPCXX ?= $(UPCXX_INSTALL)/bin/upcxx
endif
ifeq ($(UPC_INSTALL)$(UPCC),)
  $(warning UPC_INSTALL environment variable is not set, assuming upcc is in the PATH)
  UPCC = upcc
else
  UPCC ?= $(UPC_INSTALL)/bin/upcc
endif

UPC_FLAGS= $(CFLAGS)
UPCXX_FLAGS= $(CXXFLAGS)

ifeq ($(strip $(UPCXX_THREADMODE)),seq)
  UPC_THREAD_FLAGS = -nopthreads
else
  UPC_THREAD_FLAGS = -nopthreads -uses-threads
endif

UPCC_CMD  = $(shell echo $(UPCC))  $(OPTLVL) -network $(UPCXX_NETWORK) $(UPC_THREAD_FLAGS) $(UPC_FLAGS)
UPCXX_CMD = $(shell echo $(UPCXX)) $(OPTLVL) $(UPCXX_FLAGS)

# ===========================================

export UPCXX_NETWORK

PTH_TARGETS = pth_main_upc pth_arrval_upc
TARGETS = main_upc main_upcxx alloc arrval_upc arrval_upcxx $(PTH_TARGETS)

TEST_OBJS = $(PTH)test_upc.o $(PTH)test_upcxx.o

all: $(TARGETS)

version:
	@type -p $(UPCXX) ; exit 0
	-$(UPCXX_CMD) --version
	@type -p $(UPCC) ; exit 0
	-$(UPCC_CMD) --version	

$(TEST_OBJS) $(TARGETS:=.o): test.h

.upc.o:
	$(UPCC_CMD) -c $<

.cpp.o:
	$(UPCXX_CMD) -c $<

ifeq ($(strip $(PTH)),)
pth_%: force
	$(MAKE) PTH=pth_ UPC_THREAD_FLAGS=-pthreads UPCXX_THREADMODE=par $@
else
.INTERMEDIATE: pth_*.upc
pth_%.upc: %.upc
	ln -sf $^ $@
endif

%_upc: %_upc.o $(TEST_OBJS)
	$(UPCC_CMD) -link-with='$(UPCXX_CMD)' $^ -o $@

%_upcxx: %_upcxx.o $(TEST_OBJS)
	$(UPCC_CMD) -link-with='$(UPCXX_CMD)' -extern-main $^ -o $@

alloc.o: ../alloc.cpp
	$(UPCXX_CMD) -c $< -DUPC_INTEROP

alloc: alloc.o test_upc.o
	$(UPCC_CMD) -link-with='$(UPCXX_CMD)' -extern-main $^ -o $@


clean: force
	rm -f *.o $(TARGETS)

UPCXX_RUN ?= upcxx-run
TEST_NODES ?= 2
TEST_PROCS ?= 8
TEST_UPC_SZ ?= 64
TEST_UPCXX_SZ ?= 32
TEST_ARGS_alloc ?= 10 32
export TEST_ARGS_alloc

run: force $(TARGETS)
	@for exe in $(TARGETS) ; do \
	  set -x ; \
	  eval env GASNET_MAX_SEGSIZE=$(TEST_UPC_SZ)MB UPC_SHARED_HEAP_SIZE=$(TEST_UPC_SZ)MB UPCXX_SEGMENT_MB=$(TEST_UPCXX_SZ) \
	  $(UPCXX_RUN) -N $(TEST_NODES) -n $(TEST_PROCS) $$exe \$$TEST_ARGS_$$exe ; \
	 done

.PHONY: version force do_target

.SUFFIXES: .upc
	
