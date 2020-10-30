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

  static bigarray_t bigval;
  rput(bigval, gp).wait(); // rput huge val passed on stack

  finalize();
  return 0;
}
