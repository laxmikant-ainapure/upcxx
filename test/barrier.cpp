#include <upcxx/upcxx.hpp>
#include <iostream>
#include <cstdlib>
#undef NDEBUG
#include <cassert>

#include "util.hpp"

static upcxx::global_ptr<long> gp_peer;

int main(int argc, char **argv) {
  long iters = 1000;
  if (argc > 1) iters = atol(argv[1]);
  upcxx::init();

  print_test_header();

  int nranks = upcxx::rank_n();
  int self = upcxx::rank_me();
  int peer = (self + 1) % nranks;

  if (!self) std::cout << "Running " << iters << " of barrier test on " << nranks << " ranks..." << std::endl;

  upcxx::barrier();
 
  // alloc my cell 
  upcxx::global_ptr<long> gp = upcxx::new_<long>(0);

  // send the address to the right
  upcxx::rpc(peer,[gp](){ gp_peer = gp; }).wait();

  upcxx::barrier(); // wait for all pointers to arrive

  if (gp_peer.is_null()) { // verify they did
      std::cerr << self << ": ERROR gp_peer.is_null()" << std::endl;
      abort(); 
  }

  for (long i=0; i<iters; i++) {

    upcxx::rput(i, gp_peer).wait(); // write cells to the left

    upcxx::barrier(); // puts globally complete

    // check cell value
    long curval = *(gp.local());
    if (curval != i) {
      std::cerr << self << ": ERROR iter=" << i << "  curval=" << curval << std::endl;
      abort(); 
    }

    upcxx::barrier(); // wait for global validation

  }
 
  print_test_success();

  upcxx::delete_(gp);
  upcxx::finalize();
  return 0;
}
