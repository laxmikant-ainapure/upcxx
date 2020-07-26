#include <upcxx/upcxx.hpp>

int main() {
  upcxx::init();

  char a[10][4] = {};
  upcxx::promise<> p;
  upcxx::rpc(0, upcxx::operation_cx::as_promise(p), [](char a[10][4]) {}, a);
  p.finalize().wait();
  
  
  upcxx::finalize();
}
