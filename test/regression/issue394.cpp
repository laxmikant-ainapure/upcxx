#include <upcxx/upcxx.hpp>

#include "../util.hpp"

struct B {
  int x;
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
      int x = r.template read<int>();
      return new(spot) B{x};
    }
  };
};

int main() {
  upcxx::init();
  print_test_header();

  A a{3 - upcxx::rank_me()};
  upcxx::future<B> fut =
    upcxx::rpc((upcxx::rank_me()+1)%upcxx::rank_n(),
               [](const B &b) {
                 return A{b.x};
               },
               a);
  UPCXX_ASSERT_ALWAYS(fut.wait().x == a.x);

  print_test_success();
  upcxx::finalize();
}
