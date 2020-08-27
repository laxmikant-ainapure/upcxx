#include <upcxx/upcxx.hpp>
#include "../util.hpp"

struct C {
  int x;
};

struct B {
  int x;
  struct upcxx_serialization {
    template<typename Writer>
    static void serialize(Writer &w, const B &a) {
      w.write(a.x);
    }
    template<typename Reader>
    static C* deserialize(Reader &r, void *spot) {
      return new(spot) C{r.template read<int>()};
    }
  };
};

struct A {
  int x;
  struct upcxx_serialization {
    template<typename Writer>
    static void serialize(Writer &w, const A &a) {
      w.write(a.x);
    }
    template<typename Reader>
    static B* deserialize(Reader &r, void *spot) {
      return new(spot) B{r.template read<int>()};
    }
  };
};

int main() {
  upcxx::init();
  print_test_header();

  upcxx::rpc(0,[](B const &a) {
                 assert(a.x == 10);
               }, A{10}).wait();

  print_test_success();
  upcxx::finalize();
}
