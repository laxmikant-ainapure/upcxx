#include <string>
#include <upcxx/upcxx.hpp>

struct T { 
  std::string s;
  int x;
};

struct B {
  T t;
  std::string s;
  int x;
  B(...) {}
  UPCXX_SERIALIZED_VALUES(x,s,t);
};

int main() {
  upcxx::init();

  B b;
  B b2 = upcxx::serialization_traits<B>::deserialized_value(b);
  
  upcxx::finalize();
}
