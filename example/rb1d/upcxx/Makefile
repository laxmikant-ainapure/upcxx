# To use this makefile, make sure the UPCXX variable is set to the upcxx install directory, e.g.
# make UPCXX_INSTALL=<myinstalldir> hello-world
# or
# export UPCXX_INSTALL=<myinstalldir>; make hello-world
CXX := g++

OPTIMIZATION    =  -O3
# WARNINGS = -Wpointer-bool-conversion
PPFLAGS = --std=c++11
PPFLAGS += $(shell $(UPCXX_INSTALL)/bin/upcxx-meta PPFLAGS)
PPFLAGS += $(WARNINGS)
PPFLAGS += $(OPTIMIZATION)
# PPFLAGS += -I$(UPCXX_INSTALL)/include
LDFLAGS=$(shell $(UPCXX_INSTALL)/bin/upcxx-meta LDFLAGS)
LDFLAGS += --std=c++11
PPFLAGS += $(OPTIMIZATION)
LIBFLAGS=$(shell $(UPCXX_INSTALL)/bin/upcxx-meta LIBFLAGS)

all: rb1d

OBJ = rb1d.o cmdLine.o dumpAns.o fillGhost.o getTime.o

rb1d: $(OBJ)
	$(CXX) $(PPFLAGS) $(LDFLAGS) $(LIBFLAGS) -o $@  $(OBJ) 

clean:
	rm -rf rb1d *.o

.SUFFIXES: .cpp .o
.cpp.o:
	$(CXX) $(PPFLAGS) -c $<

#	$(CXX) $(PPFLAGS) -c $<  2>&1 >/dev/null | grep -v -e '^/var/folders/*' -e '^[[:space:]]*\.section' -e '^[[:space:]]*\^[[:space:]]*~*'
#	$(CXX) $(PPFLAGS) -c $< | grep -v -e '^/var/folders/*' -e '^[[:space:]]*\.section' -e '^[[:space:]]*\^[[:space:]]*~*'
#	$(CXX) $(PPFLAGS) -c $< 2>&1 | grep -v -e '^/var/folders/*' -e '^[[:space:]]*\.section' -e '^[[:space:]]*\^[[:space:]]*~*'
