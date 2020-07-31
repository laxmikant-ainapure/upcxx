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
  double d[2];
  UPCXX_SERIALIZED_FIELDS(x,s,t,d);
};

int main() {
  upcxx::init();

  B b;
  B b2 = upcxx::serialization_traits<B>::deserialized_value(b);
  
  upcxx::finalize();
}
