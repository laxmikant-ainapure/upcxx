#
# Lists of test files relative to $(top_srcdir)
#

testprograms_seq = \
	test/hello_upcxx.cpp \
	test/atomics.cpp \
	test/collectives.cpp \
	test/dist_object.cpp \
	test/local_team.cpp \
	test/barrier.cpp \
	test/rpc_barrier.cpp \
	test/rpc_ff_ring.cpp \
	test/rput.cpp \
	test/vis.cpp \
	test/vis_stress.cpp \
	test/uts/uts_ranks.cpp

testprograms_par = \
	test/rput_thread.cpp \
	test/uts/uts_hybrid.cpp \
	test/view.cpp
