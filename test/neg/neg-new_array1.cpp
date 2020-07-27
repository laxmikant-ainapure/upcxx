#include <upcxx/upcxx.hpp>

struct S {
  int f;
  S(int x) : f(x) {}
};

int main() {
  upcxx::init();

  upcxx::global_ptr<S> p = upcxx::new_array<S>(10);
  
  upcxx::finalize();
}
