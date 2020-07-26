#include <upcxx/upcxx.hpp>

struct D {  // NOT standard layout, NOT trivial, NOT POD
  char f0;
  char &fx;
  D() : fx(f0) {}
};

int main() {
  upcxx::init();
  auto base = upcxx::new_<D>();
  upcxx::global_ptr<char> gp = upcxx_memberof(base, f0);
  upcxx::finalize();
}
