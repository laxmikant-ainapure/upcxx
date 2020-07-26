#include <upcxx/upcxx.hpp>

int main() {
  upcxx::init();

  upcxx::global_ptr<int&> gp;
  
  upcxx::finalize();
}
