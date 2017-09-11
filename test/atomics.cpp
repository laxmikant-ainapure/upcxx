#include <iostream>
#include <libgen.h>
#include <upcxx/backend.hpp>
#include <upcxx/allocate.hpp>
#include <upcxx/global_ptr.hpp>
#include <upcxx/rpc.hpp>
#include <upcxx/future.hpp>
#include <upcxx/rget.hpp>
#include <upcxx/rput.hpp>
#include <upcxx/atomic.hpp>
#include <upcxx/wait.hpp>
#include "util.hpp"

using namespace upcxx;
using namespace std;

const int ITERS = 10;
global_ptr<int64_t> counter;
// let's all hit the same rank
intrank_t target_rank = 0;
global_ptr<int64_t> target_counter;

void test_fetch_add(bool use_atomics) {
    int expected_val = rank_n() * ITERS;
	if (rank_me() == 0) {
		if (!use_atomics) {
            cout << "Test fetch_add: no atomics, expect value != " << expected_val
                 << " (with multiple ranks)" << endl;
        } else {
            cout << "Test fetch_add: atomics, expect value " << expected_val << endl;
        }
		// always use atomics to access or modify counter
        wait(atomic_put(target_counter, (int64_t)0, memory_order_relaxed));
	}
	barrier();
	for (int i = 0; i < ITERS; i++) {
		// increment the target
		if (!use_atomics) {
			auto prev = upcxx::wait(rget(target_counter));
			wait(rput(prev + 1, target_counter));
		} else {
			auto prev = wait(atomic_fetch_add<int64_t>(target_counter, 1, memory_order_relaxed));
            UPCXX_ASSERT_ALWAYS(prev >= 0 && prev < rank_n() * ITERS, "atomic_fetch_add result out of range");
		}
	}
	
	barrier();
	
	if (rank_me() == target_rank) {
        cout << "Final value is " << *counter.local() << endl;
        if (use_atomics) UPCXX_ASSERT_ALWAYS(*counter.local() == expected_val);
	}
	
	barrier();
}

void test_put_get(void) {
	if (rank_me() == 0) {
		cout << "Test puts and gets: expect a random rank number" << endl;
		// always use atomics to access or modify counter
		wait(atomic_put(target_counter, (int64_t)0, memory_order_relaxed));
	}
	barrier();

	for (int i = 0; i < ITERS * 10; i++) {
		auto v = wait(atomic_get(target_counter, memory_order_relaxed));
        UPCXX_ASSERT_ALWAYS(v >=0 && v < rank_n(), "atomic_get out of range");
		wait(atomic_put(target_counter, (int64_t)rank_me(), memory_order_relaxed));
	}

	barrier();
	
	if (rank_me() == target_rank) {
        cout << "Final value is " << *counter.local() << endl;
        UPCXX_ASSERT_ALWAYS(*counter.local() >= 0 && *counter.local() < upcxx::rank_n(),
                     "atomic put and get test result out of range");
    }
	
	barrier();
}

int main(int argc, char **argv) {
	init();

    PRINT_TEST_HEADER;
    
	if (rank_me() == target_rank) counter = allocate<int64_t>();
	
	barrier();

	// get the global pointer to the target counter
	target_counter = wait(rpc(target_rank, []() { return counter; }));

	test_fetch_add(false);
	test_fetch_add(true);
	test_put_get();

    PRINT_TEST_SUCCESS;
    
	finalize();
	return 0;
}
