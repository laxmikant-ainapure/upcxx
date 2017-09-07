#include <iostream>
#include <upcxx/backend.hpp>
#include <upcxx/allreduce.hpp>
#include <upcxx/broadcast.hpp>
#include <upcxx/wait.hpp>
#include "util.hpp"

using namespace std;

int main() {
  upcxx::init();

  if (!upcxx::rank_me()) cout << "Testing " << basename(__FILE__) << " with " << upcxx::rank_n() << " ranks" << endl;
  
  int tosend = upcxx::rank_me();

  // broadcast from each rank in turn
  for (int i = 0; i < upcxx::rank_n(); i++) {
      auto fut = upcxx::broadcast(tosend, i);
      int recv = upcxx::wait(fut);
//      cout << "Recv value is " << recv << " on " << upcxx::rank_me() << endl;
      if (recv != i) FAIL("Rank " << upcxx::rank_me() << " received " << recv << ", but expected " << i);
      upcxx::barrier();
  }
  if (!upcxx::rank_me()) cout << "broadcast test: SUCCESS" << endl;

  auto fut2 = upcxx::allreduce(tosend, plus<int>());
  int recv2 = upcxx::wait(fut2);
  int expected_val = upcxx::rank_n() * (upcxx::rank_n() - 1) / 2;
  if (recv2 != expected_val) {
      FAIL("Rank " << upcxx::rank_me() << " received " << recv2 << ", but expected " << expected_val);
  }
  upcxx::barrier();
  if (!upcxx::rank_me()) {
      cout << "allreduce test: SUCCESS" << endl;
      cout << KLGREEN << "SUCCESS" << KNORM << endl;
  }

  upcxx::finalize();
  return 0;
}
