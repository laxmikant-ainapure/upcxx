#include <upcxx/upcxx.hpp>

struct D {
  char x[4];
};

int main() {
  upcxx::init();
  auto base = upcxx::new_<D>();
  upcxx::global_ptr<char> gp = upcxx_memberof_general(base, x[2]).wait();
  upcxx::finalize();
}
