#include <upcxx/upcxx.hpp>

int main() {
  upcxx::init();

  int x = 42;
  upcxx::promise<> p;
  upcxx::rpc(0, upcxx::operation_cx::as_promise(p), [](int a, int b) {}, x);
  p.finalize().wait();
  
  
  upcxx::finalize();
}
