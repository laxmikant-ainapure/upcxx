#include <upcxx/upcxx.hpp>
using namespace upcxx;

struct bigarray_t {
  double value[1024];
};

int main() {
  init();

  global_ptr<bigarray_t> gp;
  if (!rank_me()) gp = new_<bigarray_t>();
  gp = broadcast(gp, 0).wait();

  rget(gp).wait(); // rget huge val into a future

  finalize();
  return 0;
}
