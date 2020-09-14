#include <upcxx/upcxx.hpp>
using namespace upcxx;

struct bigarray_t {
  char value[256];
};

int main() {
  init();

  future<bigarray_t,bigarray_t,bigarray_t> f;
  static auto t = f.wait();

  finalize();
  return 0;
}
