#include <upcxx/upcxx.hpp>
#include <vector>
#include "../util.hpp"

struct foo {
  char a; int b; char c;
  UPCXX_SERIALIZED_FIELDS(a,b,c)
};

int main() {
  upcxx::init();

  print_test_header();

  std::vector<foo> data[2]{
    {{'a',0xb,0xc},{0xa,'b',0xc},{0xa,0xb,'c'}},
    {{'a',0xb,0xc},{0xa,'b',0xc},{0xa,0xb,'c'}}
  };

  upcxx::rpc(upcxx::rank_me(),
    [=](upcxx::view<std::vector<foo>> v) {
      foo got = (*++v.begin())[0]; // grab 0'th elt from second vector
      UPCXX_ASSERT_ALWAYS(got.a=='a' && got.b==0xb && got.c==0xc);
    },
    upcxx::make_view(data, data+2)
  ).wait();

  print_test_success();
  
  upcxx::finalize();
}
