#include <vector>
#include <upcxx/upcxx.hpp>

int main() {
  upcxx::init();

  std::vector<int> v;
  upcxx::global_ptr<std::vector<int>> gptr;
  upcxx::rget(gptr, &v, 1).wait();
  
  upcxx::finalize();
}
