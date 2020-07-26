#include <upcxx/upcxx.hpp>

struct S {
  int f;
  ~S() = delete;
};

int main() {
  upcxx::init();

  upcxx::global_ptr<S> p = upcxx::new_array<S>();
  upcxx::delete_array(p);
  
  upcxx::finalize();
}
