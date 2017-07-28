#include <upcxx/rpc.hpp>

#include <cstdint>

using namespace upcxx;
using namespace std;

// Barrier state bitmasks.
uint64_t state_bits[2] = {0, 0};

struct barrier_action {
  int epoch;
  intrank_t round;
  
  void operator()() {
    uint64_t bit = uint64_t(1)<<round;
    UPCXX_ASSERT(0 == (state_bits[epoch & 1] & bit));
    state_bits[epoch & 1] |= bit;
  }
  
  // number of empty bytes to add to messages
  size_t extra() const {
    constexpr uint32_t knuth = 0x9e3779b9u;
    // random number in [-128, 128)
    int perturb = -128 + (knuth*uint32_t(100*epoch + round) >> (32-8));
    // about half the time we'll do a rendezvous
    return backend::gasnet1_seq::am_size_rdzv_cutover + perturb - sizeof(barrier_action);
  }
};

namespace upcxx {
  // Specialize packing for barrier_action's to make random length
  // messages.
  template<>
  struct packing<barrier_action> {
    static void size_ubound(parcel_layout &ub, barrier_action x) {
      ub.add_bytes(
        sizeof(barrier_action),
        alignof(barrier_action)
      );
      ub.add_bytes(x.extra(), 1); // empty bytes
    }
    
    static void pack(parcel_writer &w, barrier_action x) {
      w.put_trivial_aligned(x);
      w.put(x.extra(), 1); // empty bytes
    }
    
    static barrier_action unpack(parcel_reader &r) {
      barrier_action x = r.get_trivial_aligned<barrier_action>();
      r.get(x.extra(), 1); // empty bytes
      return x;
    }
  };
}

void rpc_barrier() {
  intrank_t rank_n = upcxx::rank_n();
  intrank_t rank_me = upcxx::rank_me();
  
  static unsigned epoch_bump = 0;
  int epoch = epoch_bump++;
  
  intrank_t round = 0;
  
  while(1<<round < rank_n) {
    uint64_t bit = uint64_t(1)<<round;
    
    intrank_t peer = rank_me + bit;
    if(peer >= rank_n) peer -= rank_n;
    
    #if 1
      // Use random message sizes
      upcxx::rpc_ff(peer, barrier_action{epoch, round});
    #else
      // The more concise lambda way, none of the barrier_action code
      // is necessary.
      upcxx::rpc_ff(peer, [=]() {
        state_bits[epoch & 1] |= bit;
      });
    #endif
    
    while(0 == (state_bits[epoch & 1] & bit))
      upcxx::progress();
    
    round += 1;
  }
  
  state_bits[epoch & 1] = 0;
}

int main() {
  upcxx::init();
  
  intrank_t rank_me = upcxx::rank_me();
  intrank_t rank_n = upcxx::rank_n();
  
  for(int i=0; i < 10; i++) {
    rpc_barrier();
    
    if(i % rank_n == rank_me) {
      std::cout << "Barrier "<<i<<"\n";
      std::cout.flush();
    }
  }
  
  intrank_t right = (rank_me + 1) % rank_n;
  intrank_t left = (rank_me + 1 + rank_n) % rank_n;
  
  {
    future<int> fut = upcxx::rpc(right, []() {
      std::cout << "From left\n";
      std::cout.flush();
      return 0xbeef;
    });
    
    while(!fut.ready())
      upcxx::progress();
    
    UPCXX_ASSERT(fut.result() == 0xbeef);
  }
  
  rpc_barrier();
  
  if(rank_me == 0) {
    std::cout << "Eyeball me! No 'rights' before this message, no 'lefts' after.\n";
    std::cout.flush();
  }
  
  rpc_barrier();
  
  {
    future<int> fut = upcxx::rpc(left, [=]() {
      std::cout << "From right\n";
      std::cout.flush();
      return rank_me;
    });
    
    while(!fut.ready())
      upcxx::progress();
    
    UPCXX_ASSERT(fut.result() == rank_me);
  }
  
  upcxx::finalize();
  return 0;
}
