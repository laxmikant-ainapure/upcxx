#include <upcxx/upcxx.hpp>

int main() {
  upcxx::init();

  int a[10] = {};
  upcxx::rpc_ff(0, upcxx::source_cx::as_buffered(), [](int a[10]) {}, a);
  
  
  upcxx::finalize();
}
