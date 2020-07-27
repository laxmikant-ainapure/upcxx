#include <string>
#include <upcxx/upcxx.hpp>

int main() {
  upcxx::init();

  std::string s;
  upcxx::broadcast(s, 0).wait();
  
  upcxx::finalize();
}
