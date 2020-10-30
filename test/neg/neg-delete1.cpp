#include <upcxx/upcxx.hpp>

struct S {
  int f;
  ~S() = delete;
};

int main() {
  upcxx::init();

  upcxx::global_ptr<S> p = upcxx::new_<S>();
  upcxx::delete_(p);
  
  upcxx::finalize();
}
