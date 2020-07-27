#include <tuple>
#include <upcxx/upcxx.hpp>

int main() {
  upcxx::init();

  std::tuple<int> t;
  upcxx::reduce_all(t, upcxx::op_fast_add).wait();
  
  upcxx::finalize();
}
