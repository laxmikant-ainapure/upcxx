#include <upcxx/upcxx.hpp>

int main() {
  upcxx::init();

  int x = 42;
  upcxx::rpc(upcxx::world(), 0, [](upcxx::team &t, int &v) {}, upcxx::world(), x).wait();
  
  
  upcxx::finalize();
}
