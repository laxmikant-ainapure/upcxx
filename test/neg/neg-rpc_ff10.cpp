#include <upcxx/upcxx.hpp>

struct big_t {
  double val[1024];
};

int main() {
  upcxx::init();

  static big_t big;

  upcxx::rpc_ff(upcxx::world(), 0, 
    [](big_t const & r_big) {}, std::move(big));
  
  
  upcxx::finalize();
}
