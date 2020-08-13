#include <upcxx/upcxx.hpp>

struct A { // non-Serializable
  ~A() {}
};

struct B {
  A a;
  struct upcxx_serialization {
    template<typename Writer>
    static void serialize(Writer &w, const B &x) {
      w.write(x.a);
    }
    template<typename Reader>
    static B* deserialize(Reader &r, void *spot) {
      return new(spot) B;
    }
  };
};

int main() {
  upcxx::init();

  B b;
  upcxx::rpc(0, [](B b) {}, b).wait();

  upcxx::finalize();
}
