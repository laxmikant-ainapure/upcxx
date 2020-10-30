#include <upcxx/upcxx.hpp>

struct D {
  char x;
  char &ref;
  D() : ref(x) {}
};

int main() {
  upcxx::init();
  auto base = upcxx::new_<D>();
  upcxx::global_ptr<char> gp = upcxx_memberof_general(base, ref).wait();
  upcxx::finalize();
}
