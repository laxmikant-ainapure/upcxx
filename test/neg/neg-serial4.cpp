#include <upcxx/upcxx.hpp>

struct A {
  int a;
  virtual void foo() = 0;
};

struct B : public A {
 int x;
 void foo(){}
 UPCXX_SERIALIZED_VALUES(UPCXX_SERIALIZED_BASE(A), x);
 B(A &&base, int myx): A(base), x(myx) {}
 B(){}
};

int main() {
  upcxx::init();

  B b;
  B b2 = upcxx::serialization_traits<B>::deserialized_value(b);
  
  upcxx::finalize();
}
