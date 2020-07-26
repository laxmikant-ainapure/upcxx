#include <upcxx/upcxx.hpp>

int main() {
  upcxx::init();

  double a[10] = {};
  upcxx::rpc(0, [](double a[10]) {}, a).wait();
  
  
  upcxx::finalize();
}
