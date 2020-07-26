#include <upcxx/upcxx.hpp>

int main() {
  upcxx::init();

  int a[10] = {};
  upcxx::rpc_ff(0, [](int a[10]) {}, a);
  
  
  upcxx::finalize();
}
