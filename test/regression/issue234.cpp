#include <upcxx/upcxx.hpp>
#include <iostream>
#include <cassert>

using namespace upcxx;

int main() {
  init();

  int me = rank_me();
  
  promise<int> pi;
  reduce_all(rank_me(), op_fast_max, world(), operation_cx::as_promise(pi));
  
  promise<int> pi2;
  reduce_one(rank_me(), op_fast_max, 0, world(), operation_cx::as_promise(pi2));

#if 1
  promise<> p;
  barrier_async(world(), operation_cx::as_promise(p));

  promise<int> pi3;
  broadcast(42, 0, world(), operation_cx::as_promise(pi3));

  p.finalize().wait();

  int res3 = pi3.finalize().wait();
  assert(res3 == 42);
#endif

  int res = pi.finalize().wait();
  assert(res == (rank_n()-1));

  int res2 = pi2.finalize().wait();
  if (!me) assert(res2 == (rank_n()-1));

  barrier();
  if (!me) std::cout << "SUCCESS" << std::endl;
  finalize();
  return 0;
}
