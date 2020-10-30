#include <upcxx/upcxx.hpp>
#include <iostream>
#include <assert.h>
#include <unistd.h>

using namespace upcxx;

struct myclass {
   atomic_domain<int> ad;
   myclass(const upcxx::team &myteam) : ad({atomic_op::fetch_add}, myteam) {
   }
   ~myclass() {
     ad.destroy(); // includes a barrier
   }
};

int main() {
  upcxx::init();

  if (!rank_me()) std::cout << "Starting..." << std::endl;

  global_ptr<int> gp = rank_me() ? nullptr : new_<int>(0);
  gp = broadcast(gp, 0).wait();

  if (!rank_me()) { sleep(1); rpc((rank_me()+1)%rank_n(),[](){}).wait(); }

  #if FORCE_BARRIER
    upcxx::barrier();
  #endif

  { myclass mc(world());

    mc.ad.fetch_add(gp, 1, std::memory_order_acq_rel).wait();
  }

  if (!rank_me()) {
    assert(*gp.local() == rank_n());
    delete_(gp);
    std::cout << "SUCCESS" << std::endl;
  }

  upcxx::finalize();
}
