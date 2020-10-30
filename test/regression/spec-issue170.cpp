#include <upcxx/upcxx.hpp>

#include "../util.hpp"

#include <assert.h>

int main() {
  upcxx::init();
  print_test_header();

  assert(!upcxx::in_progress());

  int peer = (upcxx::rank_me() + 1) % upcxx::rank_n();

  upcxx::dist_object<int> dobj(0);
  dobj.fetch(peer).then([](int) { assert(upcxx::in_progress()); }).wait();
  assert(!upcxx::in_progress());
  upcxx::rpc(peer, []() { assert(upcxx::in_progress()); }).wait();
  assert(!upcxx::in_progress());
  
  print_test_success();
  upcxx::finalize();
  return 0;
}
