#include <upcxx/backend.hpp>
#include <upcxx/rpc.hpp>

#include <iostream>

#include "util.hpp"

bool done = false;

void arrive(upcxx::intrank_t origin) {
  if (upcxx::rank_me() == origin)
    done = true;
  else {
    upcxx::intrank_t nebr = (upcxx::rank_me() + 1)%upcxx::rank_n();
    
    upcxx::rpc_ff(nebr, [=]() {
      arrive(origin);
    });
  }
}

int main() {
  upcxx::init();

  PRINT_TEST_HEADER;
  
  upcxx::intrank_t me = upcxx::rank_me();
  upcxx::intrank_t nebr = (me + 1) % upcxx::rank_n();
  
  upcxx::rpc_ff(nebr, [=]() {
    arrive(me);
  });
  
  while(!done)
    upcxx::progress();
  
  std::cout<<"Rank " << me << " done\n";

  PRINT_TEST_SUCCESS;
  
  upcxx::finalize();
  return 0;
}
