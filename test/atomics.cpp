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
			if (prev < 0 || prev >= rank_n() * ITERS) 
                FAIL("rank " << upcxx::rank_me() << " got unexpected previous value " << prev);
		}
	}
	
	barrier();
	
	if (rank_me() == target_rank) {
        cout << "Final value is " << *counter.local() << endl;
		if (*counter.local() != expected_val && use_atomics) 
			FAIL("final value is " << *counter.local() << ", but expected " << (rank_n() * ITERS));
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
		if (v < 0 || v >= rank_n())	
			FAIL("rank " << upcxx::rank_me() << " got unexpected value " << v);
		wait(atomic_put(target_counter, (int64_t)rank_me(), memory_order_relaxed));
	}

	barrier();
	
	if (rank_me() == target_rank) {
        cout << "Final value is " << *counter.local() << endl;
        if (*counter.local() < 0 || *counter.local() >= upcxx::rank_n()) {
            FAIL("final value is out of range, " << *counter.local()
                 << " not in [0, " << upcxx::rank_n() << ")");
        }
    }
	
	barrier();
}

int main(int argc, char **argv) {
	init();

    if (!rank_me()) {
        cout << "Testing " << basename(const_cast<char*>(__FILE__)) << " with "
             << rank_n() << " ranks" << endl;
    }
	if (rank_me() == target_rank) counter = allocate<int64_t>();
	
	barrier();

	// get the global pointer to the target counter
	target_counter = wait(rpc(target_rank, []() { return counter; }));

	test_fetch_add(false);
	test_fetch_add(true);
	test_put_get();

    if (!rank_me()) cout << KLGREEN << "SUCCESS" << KNORM << endl;
    
	finalize();
	return 0;
}
