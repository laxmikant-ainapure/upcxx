#
# Lists of test files relative to $(top_srcdir)
#

###
# Section 1: user tests
# Built by 'make tests` and `make check`
###

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

###
# Section 2: developer tests
# Built by 'make tests-dev` and `make check-dev`
#
# The logic which follows sets variables which controls what is tested.
# The list of tests is constructed as follows:
#  1. A list is made of all `.cpp` files directories listed in `test_dirs`.
#  2. The variable `test_exclude_all` is initialized with matches from step 1
#     which are not valid tests, plus $(TEST_EXCLUDE_ALL) which may be provided
#     in the environment or on the command line, while seq- and par-specific
#     exclusion lists are initialized from $(TEST_EXCLUDE_SEQ) and
#     $(TEST_EXCLUDE_PAR).  These vars are whitespace-separted lists.
#  3. A series of `ifeq/ifneq` conditionals adds to `test_exclude_all`,
#     `test_exclude_seq` and `test_exclude_par`,  based on things like codemode,
#     and cuda-support.  This use of conditionals is designed to be extensible
#     to, for instance, express tests only valid for CUDA+par+debug.
#     NOTE: threadmode is handled via distinct filter variables, rather then
#     conditionals, due to the way seq and par are merged into a single list
#     for use by the "main" testing logic.
#  4. An "second round" of conditionals handles enumeration of known failures.
#     Unlike step #3, this is intended to express the status of the
#     implementation rather than a property of the test.
###

#
# Section 2. Step 1.
# List of directories containing .cpp files (searched NON-recursively):
#
test_dirs = \
	test \
	test/regression \
	test/uts \
	example/prog-guide \
	example/serialization

#
# Section 2. Step 2.
# Exclusion of files with .cpp suffix which are not suited to `make check`
# Some are pieces of a multi-file test or example.
# Others are not UPC++ tests, are intended to fail, or are not intended to be run.
#
test_exclude_all = \
	$(TEST_EXCLUDE_ALL) \
	test/segment_allocator.cpp \
	test/debug-codemode.cpp \
	test/o3-codemode.cpp \
	test/seq-threadmode.cpp \
	test/par-threadmode.cpp \
	test/multifile.cpp \
	test/multifile-buddy.cpp \
	test/promise_multiple_results.cpp \
	test/promise_reused.cpp \
	test/quiescence_failure.cpp \
	test/regression/issue105.cpp \
	test/uts/uts.cpp \
	example/prog-guide/rb1d-check.cpp

test_exclude_seq = \
	$(TEST_EXCLUDE_SEQ)

test_exclude_par = \
	$(TEST_EXCLUDE_PAR)

#
# Section 2. Step 3.
# Conditional exclusion
#

# Unconditionally exclude tests which do not use GASNet and thus
# cannot (in general) be run via `make dev-check`.
# TODO: distinguish `dev-tests` from `dev-check` to make this
# exclusion conditional, thus allowing for compile-only tests
test_exclude_all += \
	test/hello.cpp \
	test/hello_threads.cpp \
	test/lpc_barrier.cpp \
	test/uts/uts_omp.cpp \
	test/uts/uts_threads.cpp

# Conditionally exclude tests that require CUDA support:
ifeq ($(strip $(UPCXX_CUDA)),)
test_exclude_all += \
	example/prog-guide/h-d.cpp \
	example/prog-guide/h-d-remote.cpp
endif

# Conditionally exclude tests that require OpenMP:
# TODO: configure logic to enable UPCXX_HAVE_OPENMP
ifeq ($(strip $(UPCXX_HAVE_OPENMP)),)
test_exclude_all += \
	test/rput_omp.cpp \
	test/uts/uts_omp_ranks.cpp
else
# Note use of export to ensure shell can use these
export TEST_FLAGS_RPUT_OMP = $(UPCXX_OPENMP_FLAGS)
export TEST_FLAGS_UTS_OMP_RANKS = $(UPCXX_OPENMP_FLAGS)
export OMP_NUM_THREADS ?= 4
endif

# Conditionally exclude based on UPCXX_CODEMODE
ifeq ($(strip $(UPCXX_CODEMODE)),debug)
# Opt-only tests (to exclude when CODEMODE=debug)
test_exclude_all += \
	# CURRENTLY NONE
else
# Debug-only tests (to exclude when CODEMODE=opt)
test_exclude_all += \
	# CURRENTLY NONE
endif

# Conditionally exclude based on UPCXX_THREADMODE
# NOTE use of distinct variables rather than `ifeq`
# SEQ-only tests:
test_exclude_par += \
	test/regression/issue133.cpp
# PAR-only tests:
test_exclude_seq += \
	test/hello_threads.cpp \
	test/lpc_barrier.cpp \
	test/rput_thread.cpp \
	test/rput_omp.cpp \
	test/view.cpp \
	test/regression/issue142.cpp \
	test/regression/issue168.cpp \
	test/uts/uts_hybrid.cpp \
	test/uts/uts_omp_ranks.cpp \
	example/prog-guide/persona-example.cpp \
	example/prog-guide/persona-example-rputs.cpp \
	example/prog-guide/view-matrix-tasks.cpp

#
# Section 2. Step 4.
# Exclusion of tests which are "valid tests" but known to fail.
#

test_exclude_fail_all = \
	test/regression/issue242.cpp \
	test/regression/issue336.cpp \
	test/regression/issue355.cpp

test_exclude_fail_seq =

test_exclude_fail_par =

ifeq ($(strip $(GASNET_CXX_FAMILY)).$(strip $(UPCXX_CODEMODE)),PGI.debug)
test_exclude_fail_all += \
	test/regression/issue138.cpp
endif

# 
# Section 3. 
# Flags to add to specific tests
# Note use of export to ensure shell can use these
#
export TEST_FLAGS_ISSUE138=-DMINIMAL

TEST_FLAGS_MEMBEROF_PGI=--diag_suppress1427
TEST_FLAGS_MEMBEROF_GNU=-Wno-invalid-offsetof
TEST_FLAGS_MEMBEROF_Clang=-Wno-invalid-offsetof
export TEST_FLAGS_MEMBEROF=$(TEST_FLAGS_MEMBEROF_$(GASNET_CXX_FAMILY))

#
# End of configuration
#

# exclude untracked files, if any
ifneq ($(wildcard $(upcxx_src)/.git),)
test_exclude_all += $(shell cd $(upcxx_src) && git ls-files --others -- $(test_dirs) | grep '\.cpp$$')
endif

# compose the pieces above
tests_raw = $(subst $(upcxx_src)/,,$(foreach dir,$(test_dirs),$(wildcard $(upcxx_src)/$(dir)/*.cpp)))
tests_filter_out_seq = $(test_exclude_all) $(test_exclude_seq) $(test_exclude_fail_all) $(test_exclude_fail_seq)
tests_filter_out_par = $(test_exclude_all) $(test_exclude_par) $(test_exclude_fail_all) $(test_exclude_fail_par)
testprograms_dev_seq = $(filter-out $(tests_filter_out_seq),$(tests_raw))
testprograms_dev_par = $(filter-out $(tests_filter_out_par),$(tests_raw))
