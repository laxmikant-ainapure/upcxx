#include <iostream>
#include <upcxx/backend.hpp>
#include <upcxx/allreduce.hpp>
#include <upcxx/broadcast.hpp>

#include "util.hpp"

using namespace std;

int main() {
  upcxx::init();

  print_test_header();
  
  int tosend = upcxx::rank_me();

  // broadcast from each rank in turn
  for (int i = 0; i < upcxx::rank_n(); i++) {
      auto fut = upcxx::broadcast(tosend, i);
      int recv = fut.wait();
      UPCXX_ASSERT_ALWAYS(recv == i, "Received wrong value from broadcast");
      upcxx::barrier();
  }
  if (!upcxx::rank_me()) cout << "broadcast test: SUCCESS" << endl;

  auto fut2 = upcxx::allreduce(tosend, plus<int>());
  int recv2 = fut2.wait();
  int expected_val = upcxx::rank_n() * (upcxx::rank_n() - 1) / 2;
  UPCXX_ASSERT_ALWAYS(recv2 == expected_val, "Received wrong value from allreduce");
  upcxx::barrier();
  if (!upcxx::rank_me()) cout << "allreduce test: SUCCESS" << endl;

  print_test_success();
  upcxx::finalize();
  return 0;
}
