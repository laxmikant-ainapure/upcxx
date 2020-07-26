#include <upcxx/upcxx.hpp>

struct S {
  int f;
  S(){}
  S(S&& other) {}
};

int main() {
  upcxx::init();

  S s;
  upcxx::rpc_ff(upcxx::world(), 0, upcxx::source_cx::as_future(), [](S sr) {}, s).wait();
  
  
  upcxx::finalize();
}
