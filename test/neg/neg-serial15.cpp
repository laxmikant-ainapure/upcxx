#include <upcxx/upcxx.hpp>

struct bigarray_t {
  char value[8*1024];
};

struct A {
  bigarray_t b; 
  struct upcxx_serialization {
    template<typename Writer>
    static void serialize(Writer &w, const A &o) {
      w.write(o.b);
    }
    template<typename Reader>
    static A* deserialize(Reader &r, void *spot) {
      static bigarray_t x = r.template read<bigarray_t>();
      return new(spot) A;
    }
  };
};

int main() {
  upcxx::init();

  static A a;
  a = upcxx::serialization_traits<A>::deserialized_value(a);

  upcxx::finalize();
}
