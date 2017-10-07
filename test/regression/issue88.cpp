// See: https://bitbucket.org/berkeleylab/upcxx/issues/88/progress-internal-advances-the-state-of

#include <upcxx/backend.hpp>
#include <upcxx/rpc.hpp>

#include "../util.hpp"

#include <cstdlib>
#include <iostream>

int main() {
  upcxx::init();
  print_test_header();

  if(upcxx::rank_n() < 2) {
    if(upcxx::rank_me() == 0) {
      std::cerr << "Test requires 2 ranks.\n";
      std::abort();
    }
  }
  
  bool success = true;

  if(upcxx::rank_me() == 0) {
    upcxx::future<int> got = upcxx::rpc(1, [=]() { return upcxx::rank_me(); });
    
    int countdown = 10*1000*1000;
    while(!got.ready() && --countdown)
      upcxx::progress(upcxx::progress_level::internal);
    
    success = countdown == 0;
  }
  
  print_test_success(success);
  upcxx::finalize();
  return 0;
}
