#include <string>
#include <upcxx/upcxx.hpp>

int main() {
  upcxx::init();

  upcxx::global_ptr<int> gp = upcxx::new_<int>();
  int x = 42;
  upcxx::rput(0, gp,
    upcxx::remote_cx::as_rpc(
        [](int &x) {}, x));
  
  upcxx::finalize();
}
