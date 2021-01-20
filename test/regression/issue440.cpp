#include <upcxx/upcxx.hpp>
#include <iostream>
#include <assert.h>

using namespace upcxx;

struct A {
  global_ptr<int> g;
  bool local;
  A(global_ptr<int> gp) { 
   this->g = gp;
   this->local = gp.is_local();
  }
  UPCXX_SERIALIZED_VALUES(g);
};

int main() {
  upcxx::init();
  
  if (upcxx::rank_me()) {
    auto f = upcxx::rpc(0,[]() {
      global_ptr<int> gp = upcxx::new_<int>();
      A a(gp);
      return a;
    });
    A const &a = f.wait_reference();
    assert(a.g);
  }
 
  upcxx::barrier();
  if (!upcxx::rank_me()) std::cout<<"SUCCESS"<<std::endl;
  upcxx::finalize();
  return 0;
}
