#include <string>
#include <upcxx/upcxx.hpp>

int main() {
  upcxx::init();

  upcxx::global_ptr<std::string> gptr;
  upcxx::rget(gptr).wait();
  
  upcxx::finalize();
}
