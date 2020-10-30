#include <string>
#include <upcxx/upcxx.hpp>

struct S {
  int f;
  std::string s;
};

int main() {
  upcxx::init();

  S s;
  upcxx::rpc(upcxx::world(), 0, [](S sr) {}, s).wait();
  
  
  upcxx::finalize();
}
