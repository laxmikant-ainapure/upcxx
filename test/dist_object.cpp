#include <upcxx/dist_object.hpp>
#include <upcxx/backend.hpp>
#include <upcxx/rpc.hpp>

#include <iostream>

using namespace std;
using namespace upcxx;

int main() {
  upcxx::init();
  
  intrank_t me = upcxx::rank_me();
  intrank_t n = upcxx::rank_n();
  intrank_t nebr = (me + 1) % n;
  
  { // dist_object lifetime scope
    upcxx::dist_object<int> obj1{100 + me};
    upcxx::dist_object<int> obj2{200 + me};
    upcxx::dist_object<int> obj3{300 + me};
    
    future<int> f = when_all(
      upcxx::rpc(nebr,
        upcxx::bind(
          [=](dist_object<int> &his1, dist_object<int> &his2) {
            cout << me << "'s nebr values = "<< *his1 << ", " << *his2 << '\n';
          },
          obj1
        ),
        obj2
      ),
      obj3.fetch(nebr)
    );
    
    while(!f.ready())
      upcxx::progress();
    
    upcxx::barrier(); // ensures dist_object lifetime
  }
  
  upcxx::finalize();
  return 0;
}
