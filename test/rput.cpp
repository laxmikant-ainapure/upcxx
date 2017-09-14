#include <upcxx/allocate.hpp>
#include <upcxx/global_ptr.hpp>
#include <upcxx/rput.hpp>
#include <upcxx/rget.hpp>
#include <upcxx/rpc.hpp>

#include "util.hpp"

using upcxx::global_ptr;
using upcxx::intrank_t;
using upcxx::future;
using upcxx::operxn_cx_as_future;
using upcxx::source_cx_as_future;
using upcxx::remote_cx_as_rpc;

global_ptr<int> my_thing;
int got_rpc = 0;

int main() {
  upcxx::init();

  print_test_header();
  
  intrank_t me = upcxx::rank_me();
  intrank_t n = upcxx::rank_n();
  intrank_t nebr = (me + 1) % n;
  
  my_thing = upcxx::allocate<int>();
  
  upcxx::barrier();
  
  global_ptr<int> nebr_thing; {
    future<global_ptr<int>> fut = upcxx::rpc(nebr, []() { return my_thing; });
    nebr_thing = fut.wait();
  }
  
  future<> done_g, done_s;

  std::tie(done_g, done_s) = upcxx::rput(
    /*value*/100 + me,
    nebr_thing,
    operxn_cx_as_future |
    source_cx_as_future |
    remote_cx_as_rpc([=]() { got_rpc++; })
  );
  
  int buf;
  done_g = done_g.then([&]() {
    return upcxx::when_all(
        upcxx::rget(nebr_thing),
        upcxx::rget(nebr_thing, &buf, 1,
                    operxn_cx_as_future |
                    remote_cx_as_rpc([=](){ got_rpc++; })
        )
      ).then([&](int got) {
        UPCXX_ASSERT(got == 100 + me, "got incorrect value, " << got << " != " << (100 + me));
        UPCXX_ASSERT(got == buf, "got not equal to buf");
        std::cout << "get(put(X)) == X\n";
      });
  });
  
  done_g.wait();
  
  while(got_rpc != 2)
    upcxx::progress();
  
  //upcxx::barrier();
  
  upcxx::deallocate(my_thing);

  print_test_success();
  
  upcxx::finalize();
  return 0;
}
