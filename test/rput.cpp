#include <upcxx/rput.hpp>
#include <upcxx/rget.hpp>
#include <upcxx/rpc.hpp>

using namespace upcxx;

int *my_thing;
int got_rpc = -1;

int main() {
  upcxx::init();
  
  intrank_t me = upcxx::rank_me();
  intrank_t n = upcxx::rank_n();
  intrank_t nebr = (me + 1) % n;
  
  my_thing = (int*)upcxx::allocate(sizeof(int));
  
  upcxx::barrier();
  
  int *nebr_thing; {
    future<int*> fut = upcxx::rpc(nebr, []() { return my_thing; });
    while(!fut.ready()) {
      upcxx::progress();
    }
    nebr_thing = fut.result();
  }
  
  future<> done_g, done_s;
  
  std::tie(done_g, done_s) = upcxx::rput(
    /*value*/100 + me,
    /*rank_dest*/nebr, /*ptr_dest*/nebr_thing,
    operxn_cx_as_future |
    source_cx_as_future |
    remote_cx_as_rpc([=]() { got_rpc = nebr; })
  );
  
  int buf;
  done_g >>= [&]() {
    return upcxx::when_all(
        upcxx::rget(nebr, nebr_thing),
        upcxx::rget(nebr, nebr_thing, &buf, 1)
      )
      >> [&](int got) {
        UPCXX_ASSERT(got == 100 + me);
        UPCXX_ASSERT(got == buf);
        std::cout << "get(put(X)) == X\n";
      };
  };
  
  while(!done_g.ready() || got_rpc != me)
    upcxx::progress();
  
  //upcxx::barrier();
  
  upcxx::deallocate(my_thing);
  upcxx::finalize();
  return 0;
}