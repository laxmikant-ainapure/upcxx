#include <string>
#include <upcxx/upcxx.hpp>

struct S {
  int f;
  std::string s;
};

int main() {
  upcxx::init();

  upcxx::rpc(0, []() { return S(); }).wait();
  
  upcxx::finalize();
}
