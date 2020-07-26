#include <upcxx/upcxx.hpp>

struct S {
  int f;
  S(const S& other) {}
  S(){}
};

int main() {
  upcxx::init();

  S s;
  upcxx::rpc_ff(upcxx::world(), 0, [](S sr) {}, s);
  
  
  upcxx::finalize();
}
