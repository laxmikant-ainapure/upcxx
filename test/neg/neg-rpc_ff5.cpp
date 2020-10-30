#include <upcxx/upcxx.hpp>

int main() {
  upcxx::init();

  int x = 42;
  upcxx::rpc_ff(0, [](int &v) {}, x);
  
  
  upcxx::finalize();
}
