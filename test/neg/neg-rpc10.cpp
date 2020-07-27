#include <string>
#include <upcxx/upcxx.hpp>

int main() {
  upcxx::init();

  std::string s;
  upcxx::rpc(upcxx::world(), 0, upcxx::operation_cx::as_future(), [](int v) {}, s).wait();
  
  
  upcxx::finalize();
}
