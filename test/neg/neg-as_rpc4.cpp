#include <upcxx/upcxx.hpp>

struct big_t {
  double val[1024];
};

int main() {
  upcxx::init();

  upcxx::global_ptr<int> gp = upcxx::new_<int>();

  static big_t big;

  upcxx::rput(0, gp,
    upcxx::remote_cx::as_rpc(
        [](big_t const & r_big) {}, std::move(big)));
  
  upcxx::finalize();
}
