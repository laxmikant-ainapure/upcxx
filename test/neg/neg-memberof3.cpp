#include <upcxx/upcxx.hpp>

struct D {
  char f[4];
};

int main() {
  upcxx::init();
  auto base = upcxx::new_<D>();
  upcxx::global_ptr<char> gp = upcxx_memberof(base, f[5]);
  upcxx::finalize();
}
