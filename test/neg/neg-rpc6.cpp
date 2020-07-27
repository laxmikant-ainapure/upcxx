#include <vector>
#include <upcxx/upcxx.hpp>

int main() {
  upcxx::init();

  upcxx::rpc(0, []() { std::vector<int> v; return upcxx::make_view(v); }).wait();
  
  upcxx::finalize();
}
