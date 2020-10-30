#include <upcxx/upcxx.hpp>
using namespace upcxx;

struct bigarray_t {
  double value[1024];
};

int main() {
  init();

  future<bigarray_t> f;
  static bigarray_t v = f.wait();

  finalize();
  return 0;
}
