#include <upcxx/allocate.hpp>
#include <upcxx/backend.hpp>
#include <upcxx/dist_object.hpp>
#include <upcxx/global_ptr.hpp>
#include <upcxx/rpc.hpp>
#include <upcxx/team.hpp>

#include "util.hpp"

using upcxx::intrank_t;
using upcxx::global_ptr;
using upcxx::dist_object;

int main() {
  upcxx::init();
  print_test_header();

  upcxx::team &locals = upcxx::local_team();

  if(upcxx::rank_me() == 0)
    std::cout<<"local_team.rank_n() = "<<locals.rank_n()<<'\n';
  upcxx::barrier();

  intrank_t peer_me = locals.rank_me();
  intrank_t peer_n = locals.rank_n();
  dist_object<global_ptr<int>> dp(upcxx::allocate<int>(peer_n));

  for(int i=0; i < peer_n; i++) {
    global_ptr<int> p = upcxx::rpc(
        locals[(peer_me + i) % peer_n],
        [=](dist_object<global_ptr<int>> &dp) {
          return *dp + i;
        },
        dp
      ).wait();

    *p.local() = upcxx::rank_me();
  }

  upcxx::barrier();

  for(int i=0; i < peer_n; i++) {
    intrank_t want = locals[(peer_me + peer_n-i) % peer_n];
    intrank_t got = dp->local()[i];
    UPCXX_ASSERT_ALWAYS(want == got, "Want="<<want<<" got="<<got);
  }

  print_test_success();
  upcxx::finalize();
}
