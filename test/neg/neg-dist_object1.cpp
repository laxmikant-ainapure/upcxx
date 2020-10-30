#include <upcxx/upcxx.hpp>

struct S { // non-Serializable
  int f;
  ~S() {} // break trivialness
};

int main() {
  upcxx::init();

  upcxx::dist_object<S> dobj(S{3});
  dobj.fetch(0).wait();
  
  upcxx::finalize();
}
