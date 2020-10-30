#include <upcxx/upcxx.hpp>

struct big_t {
  double val[1024];
};

int main() {
  upcxx::init();

  static big_t big;

  upcxx::rpc_ff(upcxx::world(), 0, upcxx::source_cx::as_future(), 
    [](big_t const & r_big) {}, std::move(big)).wait();
  
  
  upcxx::finalize();
}
