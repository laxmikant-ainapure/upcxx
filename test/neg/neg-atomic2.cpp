#include <upcxx/upcxx.hpp>

int main() {
  upcxx::init();
  
  upcxx::atomic_domain<const int> ad({upcxx::atomic_op::load});
  
  upcxx::finalize();
}
