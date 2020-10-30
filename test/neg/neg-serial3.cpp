#include <upcxx/upcxx.hpp>

struct A {
  int a;
  virtual void foo(){}
  UPCXX_SERIALIZED_FIELDS(a);
};

struct B : public A {
 int x,y,z;
 UPCXX_SERIALIZED_FIELDS(UPCXX_SERIALIZED_BASE(A), x,y,z);
};

int main() {
  upcxx::init();

  B b;
  B b2 = upcxx::serialization_traits<B>::deserialized_value(b);
  
  upcxx::finalize();
}
