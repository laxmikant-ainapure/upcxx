#include <upcxx/upcxx.hpp>


int main() {
  upcxx::init();

  int x = 42;
  upcxx::rpc_ff(upcxx::world(), 0, [](team &t, int &v) {}, upcxx::world(), x);
  
  
  upcxx::finalize();
}
