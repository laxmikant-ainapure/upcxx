#include <string>
#include <upcxx/upcxx.hpp>

int main() {
  upcxx::init();

  std::string s;
  upcxx::global_ptr<std::string> gptr;
  upcxx::rput(s, gptr).wait();
  
  upcxx::finalize();
}
