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
      A *a = new(spot) A;
      r.template read_into<bigarray_t>(&(a->b));
      return a;
    }
  };
};

int main() {
  upcxx::init();

  static A a;
  a = upcxx::serialization_traits<A>::deserialized_value(a);
  upcxx::rpc(0,[](upcxx::view<A> v){
       static A a = *v.begin();
      }, upcxx::make_view(&a, &a+1)).wait();

  upcxx::finalize();
}
