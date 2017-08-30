# To use this makefile, first carry out the following steps:
# source <installdir>/bin/upcxx_meta
# export PPFLAGS LDFLAGS LIBFLAGS

all: hello-world compute-pi compute-pi-multi-examples

hello-world: hello-world.cpp
	g++ $< ${PPFLAGS} ${LDFLAGS} ${LIBFLAGS} -o $@ 

compute-pi: compute-pi.cpp
	g++ $< ${PPFLAGS} ${LDFLAGS} ${LIBFLAGS} -o $@ 

CP_HDRS=async-distobj-accumulate.hpp distobj-accumulate.hpp global-ptrs-accumulate.hpp rpc-accumulate.hpp atomics-accumulate.hpp fetch.hpp quiesence-accumulate.hpp

compute-pi-multi-examples: compute-pi-multi-examples.cpp $(CP_HDRS)
	g++ $< ${PPFLAGS} ${LDFLAGS} ${LIBFLAGS} -o $@ 

clean:
	rm -rf hello-world compute-pi compute-pi-multi-examples

