#include <upcxx/upcxx.hpp>

struct C {  // NOT standard layout, trivial, NOT POD
  char f0;
  private:
  double x;
};

int main() {
  upcxx::init();
  auto base = upcxx::new_<C>();
  upcxx::global_ptr<char> gp = upcxx_memberof(base, f0);
  upcxx::finalize();
}
