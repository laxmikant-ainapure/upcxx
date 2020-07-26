#include <vector>
#include <upcxx/upcxx.hpp>

int main() {
  upcxx::init();

  std::vector<int> vecs[4];
  upcxx::broadcast(vecs, 4, 0).wait();
  
  upcxx::finalize();
}
