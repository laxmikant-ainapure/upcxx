#include <upcxx/upcxx.hpp>

int main() {
  upcxx::init();
  
  upcxx::atomic_domain<char> ad({upcxx::atomic_op::load});
  
  upcxx::finalize();
}
