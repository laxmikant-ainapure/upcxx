#include <upcxx/backend.hpp>
#include <upcxx/rpc.hpp>

#include <iostream>

bool done = false;

void arrive(upcxx::intrank_t origin) {
  if(upcxx::rank_me() == origin)
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
  
  upcxx::intrank_t me = upcxx::rank_me();
  upcxx::intrank_t nebr = (me + 1) % upcxx::rank_n();
  
  upcxx::rpc_ff(nebr, [=]() {
    arrive(me);
  });
  
  while(!done)
    upcxx::progress();
  
  std::cout<<"Done\n";
  
  upcxx::finalize();
  return 0;
}
