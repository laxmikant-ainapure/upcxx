#include <upcxx/barrier.hpp>

using namespace upcxx;
using namespace std;

namespace {
  constexpr int radix_log2 = 4;
  constexpr int radix = 1<<radix_log2;
  
  struct barrier_state {
    int incoming;
    upcxx::promise<> pro;
    
    static barrier_state* lookup(team &tm, digest id);
    void receive(team &tm, digest id);
    void broadcast(team &tm, digest id, intrank_t rank_ub);
  };
  
  barrier_state* barrier_state::lookup(team &tm, digest id) {
    auto it_and_inserted = detail::registry.insert({id, nullptr});
    barrier_state *st;
    
    if(it_and_inserted.second) {
      st = new barrier_state;
      it_and_inserted.first->second = (void*)st;
      
      intrank_t rank_n = tm.rank_n();
      intrank_t rank_me = tm.rank_me();
      
      int incoming = 1; // one from self
      int sh = 0;
      while(0 == (intrank_t(radix-1)<<sh & rank_me)) {
        int peers = (rank_n - rank_me + (intrank_t(1)<<sh)-1)>>sh;
        if(peers < radix) {
          incoming += peers-1;
          break;
        }
        incoming += radix-1;
        sh += radix_log2;
      }
      
      st->incoming = incoming;
    }
    else
      st = static_cast<barrier_state*>(it_and_inserted.first->second);
    
    return st;
  }
  
  void barrier_state::receive(team &tm, digest id) {
    if(--this->incoming == 0) {
      if(tm.rank_me() == 0) {
        intrank_t rank_ub = tm.rank_n();
        this->broadcast(tm, id, rank_ub);
      }
      else {
        intrank_t me = tm.rank_me();
        intrank_t parent;
        
        // Compute the parent of this rank by looking at the "digits" (wrt radix,
        // so each digit is a chunk of radix_log2 bits) of rank_me() and zeroing
        // out the least non-zero digit.
        if(0 == (radix_log2 & (radix_log2-1))) {
          // Radix is power of power of 2, use a bit twiddling trick.
          uintrank_t m = (uintrank_t)me;
          for(int i=0; (1<<i) < radix_log2; i++) // this unrolls
            m |= m>>(1<<i);
          m &= uintrank_t(-1)/(radix-1);
          m ^= m & (m-1);
          m *= radix-1;
          parent = me & ~intrank_t(m);
        }
        else {
          // Radix is power of 2 but not power of power of 2. find least
          // non-zero "digit" using loop.
          int sh = radix_log2;
          while(me == (me & ~((intrank_t(1)<<sh)-1)))
            sh += radix_log2;
          parent = me & ~((intrank_t(1)<<sh)-1);
        }
        
        team_id tm_id = tm.id();
        backend::send_am_master<progress_level::internal>(
          tm, parent,
          [=]() {
            team &tm = tm_id.here();
            barrier_state *me = barrier_state::lookup(tm, id);
            me->receive(tm, id);
          }
        );
      }
    }
  }
  
  void barrier_state::broadcast(team &tm, digest id, intrank_t rank_ub) {
    intrank_t rank_me = tm.rank_me();
    while(true) {
      intrank_t mid = rank_me + (rank_ub - rank_me)*(radix-1)/radix;
      
      if(mid == rank_me)
        break;
      
      team_id tm_id = tm.id();
      backend::template send_am_master<progress_level::internal>(
        tm, mid,
        [=]() {
          barrier_state *me = (barrier_state*)detail::registry.at(id);
          me->broadcast(tm_id.here(), id, rank_ub);
        }
      );
      
      rank_ub = mid;
    }
    
    backend::fulfill_during_user(std::move(this->pro), /*anon*/1);
    detail::registry.erase(id);
    delete this;
  }
}

#if 0
void upcxx::barrier() {
  UPCXX_ASSERT(backend::master.active_with_caller());
  
  gasnet_barrier_notify(0, GASNET_BARRIERFLAG_ANONYMOUS);
  
  while(GASNET_OK != gasnet_barrier_try(0, GASNET_BARRIERFLAG_ANONYMOUS))
    upcxx::progress();
}
#else
void upcxx::barrier(team &tm) {
  UPCXX_ASSERT(backend::master.active_with_caller());
  
  digest id = tm.next_collective_id(detail::internal_only());
  barrier_state *st = barrier_state::lookup(tm, id);
  future<> ans = st->pro.get_future();
  st->receive(tm, id);
  ans.wait();
}
#endif

template<>
future<> upcxx::barrier_async<
      completions<future_cx<operation_cx_event>>
    >(
    team &tm,
    completions<future_cx<operation_cx_event>> cxs_ignored
  ) {
  UPCXX_ASSERT(backend::master.active_with_caller());
  
  digest id = tm.next_collective_id(detail::internal_only());
  barrier_state *st = barrier_state::lookup(tm, id);
  future<> ans = st->pro.get_future();
  st->receive(tm, id);
  return ans;
}
