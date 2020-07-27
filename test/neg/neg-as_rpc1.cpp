#include <upcxx/upcxx.hpp>

int main() {
  upcxx::init();

  double a[10] = {};
  upcxx::global_ptr<int> gp = upcxx::new_<int>();
  upcxx::rput(0, gp,
    upcxx::remote_cx::as_rpc(
        [](double a[10]) {}, a));
  
  upcxx::finalize();
}
