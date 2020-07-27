#include <upcxx/upcxx.hpp>

int main() {
  upcxx::init();

  upcxx::global_ptr<volatile int> gp;
  
  upcxx::finalize();
}
