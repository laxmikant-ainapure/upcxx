#include <upcxx/upcxx.hpp>

struct A {
 int x,y,z;
 UPCXX_SERIALIZED_FIELDS(x,y,z);
};

struct B : public A {
  UPCXX_SERIALIZED_DELETE();
};

int main() {
  upcxx::init();

  B b;
  upcxx::rpc(0, [](B const &b){}, b).wait();
  
  upcxx::finalize();
}
