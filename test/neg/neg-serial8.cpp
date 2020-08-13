#include <string>
#include <upcxx/upcxx.hpp>

struct B {
  std::string s;
  int x;
  const double d[2] = {1.0, -3.1};
  UPCXX_SERIALIZED_FIELDS(x,s,d);
};

int main() {
  upcxx::init();

  B b;
  B b2 = upcxx::serialization_traits<B>::deserialized_value(b);
  
  upcxx::finalize();
}
