#include <upcxx/allocate.hpp>
#include <upcxx/global_ptr.hpp>
#include <upcxx/rput.hpp>
#include <upcxx/rget.hpp>
#include <upcxx/rpc.hpp>

#include "util.hpp"

using upcxx::global_ptr;
using upcxx::intrank_t;
using upcxx::future;
using upcxx::promise;
using upcxx::operation_cx;
using upcxx::source_cx;
using upcxx::remote_cx;

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
  
  int value = 100+me;
  #if 0
    std::tie(done_g, done_s) = upcxx::rput(
      &value, nebr_thing, 1,
      operation_cx::as_future() |
      source_cx::as_future() |
      remote_cx::as_rpc([=]() { got_rpc++; })
    );
  #else
    std::tie(done_g, done_s) = upcxx::rput(
      &value, nebr_thing, 1,
      operation_cx::as_buffered() |
      operation_cx::as_future() |
      source_cx::as_future() |
      remote_cx::as_rpc([=]() { got_rpc++; })
    );
  #endif
  
  int buf;
  done_g = done_g.then([&]() {
    promise<int> *pro1 = new promise<int>;
    upcxx::rget(nebr_thing, operation_cx::as_promise(*pro1));
    
    promise<> *pro2 = new promise<>;
    upcxx::rget(nebr_thing, &buf, 1,
                operation_cx::as_promise(*pro2) |
                remote_cx::as_rpc([=](){ got_rpc++; }));
    
    return upcxx::when_all(
        pro1->finalize(),
        pro2->finalize()
      ).then([&,pro1,pro2](int got) {
        delete pro1;
        delete pro2;

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
