#include <upcxx/upcxx.hpp>
#include <string>

int main() {
  upcxx::init();

  int x = 42;
  upcxx::rpc_ff(upcxx::world(), 0, upcxx::source_cx::as_future(), [](std::string s) {}, x).wait();
  
  
  upcxx::finalize();
}
