#include <upcxx/upcxx.hpp>

int main() {
  upcxx::init();

  int x = 42;
  upcxx::rpc(0, [](int &v) {}, x).wait();
  
  
  upcxx::finalize();
}
