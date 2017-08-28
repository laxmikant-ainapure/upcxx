#include <iostream>
#include <upcxx/backend.hpp>
#include <upcxx/allocate.hpp>
#include <upcxx/global_ptr.hpp>
#include <upcxx/rpc.hpp>
#include <upcxx/future.hpp>
#include <upcxx/rget.hpp>
#include <upcxx/rput.hpp>
#include <upcxx/atomic.hpp>
#include <upcxx/wait.hpp>

using namespace upcxx;
using namespace std;

#define KNORM  "\x1B[0m"
#define KLRED "\x1B[91m"
#define KLGREEN "\x1B[92m"


const int ITERS = 10;
global_ptr<int64_t> counter;
// let's all hit the same rank
intrank_t target_rank = 0;
global_ptr<int64_t> target_counter;

void test_fetch_add(bool use_atomics) {
	if (rank_me() == 0) {
		if (!use_atomics) cout << "Test fetch_add: no atomics, expect failure (with multiple ranks)" << endl;
		else cout << "Test fetch_add: atomics, expect pass" << endl;
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
			if (prev < 0 || prev >= rank_n() * ITERS) cout << "Unexpected previous value " << prev << endl;
		}
	}
	
	barrier();
	
	if (rank_me() == target_rank) {
		if (*counter.local() != rank_n() * ITERS) {
			cout << KLRED "FAIL" KNORM << ": final value is " << *counter.local() << ", but expected "
					 << (rank_n() * ITERS) << endl;
		} else {
			cout << KLGREEN "PASS" KNORM << ": final value is " << *counter.local() << endl;
		}
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
		if (v < 0 || v >= rank_n())	{
			cout << KLRED "FAIL" KNORM << ": unexpected value " << v << endl;
			return;
		}
		wait(atomic_put(target_counter, (int64_t)rank_me(), memory_order_relaxed));
	}

	barrier();
	
	if (rank_me() == target_rank) cout << "Final value is " << *counter.local() << endl;
	
	barrier();
}

int main() {
	init();
    
	if (rank_me() == target_rank) counter = allocate<int64_t>();
	
	barrier();

	// get the global pointer to the target counter
	target_counter = wait(rpc(target_rank, []() { return counter; }));

	test_fetch_add(false);
	test_fetch_add(true);
	test_put_get();
	
	finalize();
	return 0;
}
