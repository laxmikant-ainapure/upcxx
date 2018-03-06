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

#include "util.hpp"

using namespace std;

using upcxx::rank_me;
using upcxx::rank_n;
using upcxx::barrier;
using upcxx::global_ptr;


const int ITERS = 10;
global_ptr<int64_t> counter;
// let's all hit the same rank
const upcxx::intrank_t TARGET_RANK = 0;

void test_fetch_add(global_ptr<int64_t> target_counter, bool use_atomics, 
                    upcxx::atomic_domain<int64_t> &dom) {
  int expected_val = rank_n() * ITERS;
  if (rank_me() == 0) {
    if (!use_atomics) {
      cout << "Test fetch_add: no atomics, expect value != " << expected_val
              << " (with multiple ranks)" << endl;
    } else {
      cout << "Test fetch_add: atomics, expect value " << expected_val << endl;
    }
    
    // always use atomics to access or modify counter - alternative API
    dom.store(target_counter, (int64_t)0).wait();
  }
  barrier();
  for (int i = 0; i < ITERS; i++) {
    // increment the target
    if (!use_atomics) {
      auto prev = rget(target_counter).wait();
      rput(prev + 1, target_counter).wait();
    } else {
      // This should cause an assert failure
      //auto prev = dom.fsub(target_counter, (int64_t)1).wait();
      auto prev = dom.fetch_add(target_counter, (int64_t)1).wait();
      UPCXX_ASSERT_ALWAYS(prev >= 0 && prev < rank_n() * ITERS, 
              "atomic::fetch_add result out of range");
    }
  }
  
  barrier();
  
  if (rank_me() == TARGET_RANK) {
    cout << "Final value is " << *counter.local() << endl;
    if (use_atomics)
      UPCXX_ASSERT_ALWAYS(*counter.local() == expected_val, 
              "incorrect final value for the counter");
  }
  
  barrier();
}

void test_put_get(global_ptr<int64_t> target_counter, upcxx::atomic_domain<int64_t> &dom) {
  if (rank_me() == 0) {
    cout << "Test puts and gets: expect a random rank number" << endl;
    // always use atomics to access or modify counter
    dom.store(target_counter, (int64_t)0).wait();
  }
  barrier();
  
  for (int i = 0; i < ITERS * 10; i++) {
    auto v = dom.load(target_counter).wait();    
    UPCXX_ASSERT_ALWAYS(v >=0 && v < rank_n(), "atomic_get out of range: " << v);
    dom.store(target_counter, (int64_t)rank_me()).wait();
  }
  
  barrier();
  
  if (rank_me() == TARGET_RANK) {
    cout << "Final value is " << *counter.local() << endl;
    UPCXX_ASSERT_ALWAYS(*counter.local() >= 0 && *counter.local() < upcxx::rank_n(),
            "atomic put and get test result out of range");
  }
  
  barrier();
}

#define CHECK_ATOMIC_VAL(v, V) UPCXX_ASSERT_ALWAYS(v == V, "expected " << V << ", got " << v);

void test_all_ops(global_ptr<int64_t> target_counter, upcxx::atomic_domain<int64_t> &dom) {
  if (upcxx::rank_me() == 0) {
    dom.store(target_counter, (int64_t)42).wait();
    int64_t v = dom.load(target_counter).wait();
    CHECK_ATOMIC_VAL(v, 42);
    dom.inc(target_counter).wait();
    v = dom.fetch_inc(target_counter).wait();
    CHECK_ATOMIC_VAL(v, 43);
    dom.dec(target_counter).wait();
    v = dom.fetch_dec(target_counter).wait();
    CHECK_ATOMIC_VAL(v, 43);
    dom.add(target_counter, 7).wait();
    v = dom.fetch_add(target_counter, 5).wait();
    CHECK_ATOMIC_VAL(v, 49);
    dom.sub(target_counter, 3).wait();
    v = dom.fetch_sub(target_counter, 2).wait();
    CHECK_ATOMIC_VAL(v, 51);
    v = dom.compare_exchange(target_counter, 49, 42).wait();
    CHECK_ATOMIC_VAL(v, 49);
    v = dom.compare_exchange(target_counter, 0, 3).wait();
    CHECK_ATOMIC_VAL(v, 42);
  }
  upcxx::barrier();
}

int main(int argc, char **argv) {
  upcxx::init();
  
  upcxx::atomic_domain<int64_t> ad_i64({upcxx::atomic_op::load, upcxx::atomic_op::store, 
          upcxx::atomic_op::add, upcxx::atomic_op::fetch_add,
          upcxx::atomic_op::sub, upcxx::atomic_op::fetch_sub,          
          upcxx::atomic_op::inc, upcxx::atomic_op::fetch_inc,
          upcxx::atomic_op::dec, upcxx::atomic_op::fetch_dec,
          upcxx::atomic_op::compare_exchange});
   
  // uncomment to evaluate compile-time error checking
  //upcxx::atomic_domain<const int> ad_cint({upcxx::atomic_op::get});
  // test non-fixed-width domains


//  upcxx::atomic_domain<unsigned> ad_ui({upcxx::atomic_op::store});
//  global_ptr<unsigned> y;
//  ad_ui.store(y, (unsigned)0);
          
//  upcxx::atomic_domain<long long> ad_ll({upcxx::atomic_op::store});
//  global_ptr<long long> x = upcxx::allocate<long long>();
//}  ad_ll.store(x, (long long)0);

  upcxx::atomic_domain<long> ad_l({upcxx::atomic_op::store});
  global_ptr<long> z = upcxx::allocate<long>();
  ad_l.store(z, (long)0).wait();

  print_test_header();
  
  if (rank_me() == TARGET_RANK) counter = upcxx::allocate<int64_t>();
  
  barrier();
  
  // get the global pointer to the target counter
  global_ptr<int64_t> target_counter = upcxx::rpc(TARGET_RANK, []() { return counter; }).wait();

  test_all_ops(target_counter, ad_i64);
  test_fetch_add(target_counter, false, ad_i64);
  test_fetch_add(target_counter, true, ad_i64);
  test_put_get(target_counter, ad_i64);

  print_test_success();
  
  upcxx::finalize();
  return 0;
}
