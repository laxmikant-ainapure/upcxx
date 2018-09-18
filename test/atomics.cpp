#include <iostream>
#include <libgen.h>
#include <upcxx/backend.hpp>
#include <upcxx/barrier.hpp>
#include <upcxx/allocate.hpp>
#include <upcxx/global_ptr.hpp>
#include <upcxx/rpc.hpp>
#include <upcxx/future.hpp>
#include <upcxx/rget.hpp>
#include <upcxx/rput.hpp>
#include <upcxx/atomic.hpp>

#include "util.hpp"

using namespace std;

using upcxx::team;
using upcxx::global_ptr;


constexpr int ITERS = 10;
global_ptr<int64_t> counter;

// let's all hit the same rank
upcxx::intrank_t target_rank(team &tm) {
  return 0xbeef % tm.rank_n();
}

void test_fetch_add(team &tm, global_ptr<int64_t> target_counter, bool use_atomics, 
                    upcxx::atomic_domain<int64_t> &dom) {
  int expected_val = tm.rank_n() * ITERS;
  if (tm.rank_me() == 0) {
    if (!use_atomics) {
      cout << "Test fetch_add: no atomics, expect value != " << expected_val
              << " (with multiple ranks)" << endl;
    } else {
      cout << "Test fetch_add: atomics, expect value " << expected_val << endl;
    }
    
    // always use atomics to access or modify counter - alternative API
    dom.store(target_counter, (int64_t)0, memory_order_relaxed).wait();
  }
  upcxx::barrier(tm);
  for (int i = 0; i < ITERS; i++) {
    // increment the target
    if (!use_atomics) {
      auto prev = rget(target_counter).wait();
      rput(prev + 1, target_counter).wait();
    } else {
      // This should cause an assert failure
      //auto prev = dom.fsub(target_counter, (int64_t)1).wait();
      auto prev = dom.fetch_add(target_counter, (int64_t)1, memory_order_relaxed).wait();
      UPCXX_ASSERT_ALWAYS(prev >= 0 && prev < tm.rank_n() * ITERS, 
              "atomic::fetch_add result out of range");
    }
  }
  
  upcxx::barrier(tm);
  
  if (tm.rank_me() == target_rank(tm)) {
    cout << "Final value is " << *counter.local() << endl;
    if (use_atomics)
      UPCXX_ASSERT_ALWAYS(*counter.local() == expected_val, 
              "incorrect final value for the counter");
  }
  
  upcxx::barrier(tm);
}

void test_put_get(team &tm, global_ptr<int64_t> target_counter, upcxx::atomic_domain<int64_t> &dom) {
  if (tm.rank_me() == 0) {
    cout << "Test puts and gets: expect a random rank number" << endl;
    // always use atomics to access or modify counter
    dom.store(target_counter, (int64_t)0, memory_order_relaxed).wait();
  }
  upcxx::barrier(tm);
  
  if (tm.rank_me() == target_rank(tm)) {
    UPCXX_ASSERT_ALWAYS(target_counter.where() == upcxx::rank_me());
    UPCXX_ASSERT_ALWAYS(*target_counter.local() == 0);
  }
  upcxx::barrier(tm);
  
  for (int i = 0; i < ITERS * 10; i++) {
    auto v = dom.load(target_counter, memory_order_relaxed).wait();
    UPCXX_ASSERT_ALWAYS(v >=0 && v < tm.rank_n(), "atomic_get out of range: " << v);
    dom.store(target_counter, (int64_t)tm.rank_me(), memory_order_relaxed).wait();
  }
  
  upcxx::barrier(tm);
  
  if (tm.rank_me() == target_rank(tm)) {
    cout << "Final value is " << *target_counter.local() << endl;
    UPCXX_ASSERT_ALWAYS(
        *target_counter.local() >= 0 && *target_counter.local() < tm.rank_n(),
        "atomic put and get test result out of range: got="<<*target_counter.local()<<" range=[0,"<<tm.rank_n()<<")"
      );
  }
  
  upcxx::barrier(tm);
}

#define CHECK_ATOMIC_VAL(v, V) UPCXX_ASSERT_ALWAYS(v == V, "expected " << V << ", got " << v);

void test_all_ops(team &tm, global_ptr<int64_t> target_counter, upcxx::atomic_domain<int64_t> &dom) {
  if (tm.rank_me() == 0) {
    dom.store(target_counter, (int64_t)42, memory_order_relaxed).wait();
    int64_t v = dom.load(target_counter, memory_order_relaxed).wait();
    CHECK_ATOMIC_VAL(v, 42);
    dom.inc(target_counter, memory_order_relaxed).wait();
    v = dom.fetch_inc(target_counter, memory_order_relaxed).wait();
    CHECK_ATOMIC_VAL(v, 43);
    dom.dec(target_counter, memory_order_relaxed).wait();
    v = dom.fetch_dec(target_counter, memory_order_relaxed).wait();
    CHECK_ATOMIC_VAL(v, 43);
    dom.add(target_counter, 7, memory_order_relaxed).wait();
    v = dom.fetch_add(target_counter, 5, memory_order_relaxed).wait();
    CHECK_ATOMIC_VAL(v, 49);
    dom.sub(target_counter, 3, memory_order_relaxed).wait();
    v = dom.fetch_sub(target_counter, 2, memory_order_relaxed).wait();
    CHECK_ATOMIC_VAL(v, 51);
    v = dom.compare_exchange(target_counter, 49, 42, memory_order_relaxed).wait();
    CHECK_ATOMIC_VAL(v, 49);
    v = dom.compare_exchange(target_counter, 0, 3, memory_order_relaxed).wait();
    CHECK_ATOMIC_VAL(v, 42);
  }
  upcxx::barrier(tm);
}

void test_team(upcxx::team &tm) {
  upcxx::atomic_domain<int64_t> ad_i64(
      {upcxx::atomic_op::load, upcxx::atomic_op::store, 
       upcxx::atomic_op::add, upcxx::atomic_op::fetch_add,
       upcxx::atomic_op::sub, upcxx::atomic_op::fetch_sub,          
       upcxx::atomic_op::mul, upcxx::atomic_op::fetch_mul,
       upcxx::atomic_op::min, upcxx::atomic_op::fetch_min,
       upcxx::atomic_op::max, upcxx::atomic_op::fetch_max,
       upcxx::atomic_op::bit_and, upcxx::atomic_op::fetch_bit_and,
       upcxx::atomic_op::bit_or, upcxx::atomic_op::fetch_bit_or,
       upcxx::atomic_op::bit_xor, upcxx::atomic_op::fetch_bit_xor,
       upcxx::atomic_op::inc, upcxx::atomic_op::fetch_inc,
       upcxx::atomic_op::dec, upcxx::atomic_op::fetch_dec,
       upcxx::atomic_op::compare_exchange},
       tm
    );
  
  // uncomment to evaluate compile-time error checking
  //upcxx::atomic_domain<const int> ad_cint({upcxx::atomic_op::load});

  // check fixed-width supported integer types
  upcxx::atomic_domain<int32_t> ad_i({upcxx::atomic_op::store}, tm);
  auto xi = upcxx::allocate<int32_t>();
  ad_i.store(xi, (int32_t)0, memory_order_relaxed);
  
  upcxx::atomic_domain<uint32_t> ad_ui({upcxx::atomic_op::store}, tm);
  auto xui = upcxx::allocate<uint32_t>();
  ad_ui.store(xui, (uint32_t)0, memory_order_relaxed);
  ad_ui.destroy();
  
  upcxx::atomic_domain<int64_t> ad_l({upcxx::atomic_op::store}, tm);
  auto xl = upcxx::allocate<int64_t>();
  ad_l.store(xl, (int64_t)0, memory_order_relaxed);
  ad_l.destroy();
  
  upcxx::atomic_domain<uint64_t> ad_ul({upcxx::atomic_op::store}, tm);
  auto xul = upcxx::allocate<uint64_t>();
  ad_ul.store(xul, (uint64_t)0, memory_order_relaxed);
  ad_ul.destroy();
  
  upcxx::atomic_domain<int32_t> ad = std::move(ad_i);
  ad.store(xi, (int32_t)0, memory_order_relaxed);
  ad.destroy();
  
  // will fail with an error message about no move assignment on a non-default constructed domain
  //ad = std::move(ad_i);
  
  // this will fail with an error message about an unsupported domain
  //ad_ul.load(xul, memory_order_relaxed).wait();
  // this will fail with a null ptr message
  //ad_ul.store(nullptr, (unsigned long)0, memory_order_relaxed);
          
  if(tm.rank_me() == target_rank(tm))
    counter = upcxx::allocate<int64_t>();
  
  upcxx::barrier(tm);
  
  // get the global pointer to the target counter
  global_ptr<int64_t> target_counter = upcxx::rpc(tm, target_rank(tm), []() { return counter; }).wait();

  test_all_ops(tm, target_counter, ad_i64);
  test_fetch_add(tm, target_counter, false, ad_i64);
  test_fetch_add(tm, target_counter, true, ad_i64);
  test_put_get(tm, target_counter, ad_i64);
  ad_i64.destroy();
  
  if(tm.rank_me() == target_rank(tm))
    upcxx::deallocate(counter);
}

int main(int argc, char **argv) {
  upcxx::init();
  print_test_header();
  {
    test_team(upcxx::world());
    
    team tm0 = upcxx::world().split(upcxx::rank_me() % 3, 0);
    test_team(tm0);
    tm0.destroy();
    
    test_team(upcxx::local_team());
    
    team tm1 = upcxx::local_team().split(upcxx::world().rank_me() % 2, 0);
    test_team(tm1);
    tm1.destroy();
  }
  print_test_success();
  upcxx::finalize();
  return 0;
}
