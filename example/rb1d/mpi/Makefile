HOST = $(shell hostname)
BANG   =  $(shell expr match `hostname` ccom-bang)
COMPUTE   =  $(shell expr match `hostname` compute)
SORKEN   =  $(shell expr match `hostname` sorken)
EDISON =  $(shell expr match `hostname` edison)
CORI =  $(shell expr match `hostname` cori)

mpi := 1
ifneq ($(BANG), 0)
include $(PUB)/Arch/arch.gnu-c++11.generic
else
ifneq ($(COMPUTE), 0)
include $(PUB)/Arch/arch.gnu-c++11.generic
else
ifneq ($(SORKEN), 0)
include $(PUB)/Arch/arch.gnu-c++11.generic
else
ifneq ($(EDISON),0)
include $(PUB)/Arch/arch.intel-c++11.generic
else
ifneq ($(CORI),0)
include $(PUB)/Arch/arch.intel-c++11.generic
else
include ./arch.gnu-c++11.generic
endif
endif
endif
endif
endif

app:		rb1d


OBJ	=	rb1d.o fillGhost.o cmdLine.o dumpAns.o

rb1d:	        $(OBJ) rb1d.h
		$(C++LINK) $(LDFLAGS) -o $@ $(OBJ) -lm  $(LDLIBS)

clean:	
	$(RM) *.o rb1d;
	$(RM) core*;
