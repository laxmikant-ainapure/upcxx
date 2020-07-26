#include <string>
#include <upcxx/upcxx.hpp>

struct S {
  int f;
  std::vector<int> v;
};

int main() {
  upcxx::init();

  S s;
  upcxx::rpc(upcxx::world(), 0, upcxx::operation_cx::as_future(), [](S sr) {}, s).wait();
  
  
  upcxx::finalize();
}
