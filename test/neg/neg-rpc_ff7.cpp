#include <upcxx/upcxx.hpp>

int main() {
  upcxx::init();

  int x = 42;
  upcxx::rpc_ff(0, upcxx::source_cx::as_buffered(), [](int a, int b) {}, x);
  
  
  upcxx::finalize();
}
