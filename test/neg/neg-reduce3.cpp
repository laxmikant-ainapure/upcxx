#include <upcxx/upcxx.hpp>

int main() {
  upcxx::init();

  double d;
  upcxx::reduce_all(d, upcxx::op_fast_bit_and).wait();
  
  upcxx::finalize();
}
