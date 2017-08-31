#ifndef _b91be200_fd4d_41f5_a326_251161564ec7
#define _b91be200_fd4d_41f5_a326_251161564ec7

#include <upcxx/backend.hpp>
#include <upcxx/dist_object.hpp>
#include <upcxx/rpc.hpp>

namespace upcxx {
  namespace detail {
    // We take a dist_id instead of dist_object because even if we
    // arrive before this rank constructed its object, we can still
    // send the rpc_ff's down the tree.
    template<typename T>
    void broadcast_receive(
        T const &value,
        intrank_t rank_ub, // in range [0, 2*rank_n-1)
        dist_id<promise<T>> id
      ) {
      
      intrank_t rank_me = upcxx::rank_me();
      intrank_t rank_n = upcxx::rank_n();
      
      // Send to top-half of [rank_me,peer_ub), then set interval to
      // lower-half and repeat.
      while(true) {
        intrank_t mid = rank_me + (rank_ub-rank_me)/2;
        
        // Send-to-self is stop condition.
        if(mid == rank_me)
          break;
        
        intrank_t translate = rank_n <= mid ? rank_n : 0;
        
        // Sub-interval bounds. Lower must be in [0,rank_n).
        intrank_t sub_lb = mid - translate;
        intrank_t sub_ub = rank_ub - translate;
        
        rpc_ff(sub_lb,
          [=](T const &value) {
            broadcast_receive(value, sub_ub, id);
          },
          value
        );
        
        rank_ub = mid;
      }
      
      id.when_here().then(
        [=](dist_object<promise<T>> &state) {
          state->fulfill_result(value);
        }
      );
    }
  }
  
  template<typename T1,
           typename T = typename std::decay<T1>::type>
  future<T> broadcast(
      T1 &&value, intrank_t root
    ) {
    intrank_t rank_n = upcxx::rank_n();
    intrank_t rank_me = upcxx::rank_me();
    
    dist_object<promise<T>> *state = new dist_object<promise<T>>({});
    
    if(rank_me == root)
      detail::broadcast_receive(value, root + rank_n, state->id());
    
    future<T> ans = (*state)->get_future();
    
    // Cleanup dist_object after we've received our value.
    ans.then([state](T&) { delete state; });
    
    return ans;
  }
}
#endif
