#ifndef _fdec460f_0bc2_4c14_b88c_c85d76235cd3
#define _fdec460f_0bc2_4c14_b88c_c85d76235cd3

#include <upcxx/backend.hpp>
#include <upcxx/dist_object.hpp>
#include <upcxx/rpc.hpp>

namespace upcxx {
  namespace detail {
    template<typename T, typename Op>
    struct allreduce_state {
      int incoming;
      Op op;
      T accum;
      promise<T> answer;
      
      // Called after `accum` has been updated by child.
      void contributed(dist_object<allreduce_state> &this_obj);
      
      // Called to disseminate result value to ranks [rank_me,rank_ub).
      void broadcast(
        dist_object<allreduce_state> &this_obj,
        T const &value,
        intrank_t rank_ub
      );
    };
  }
  
  template<typename T1, typename BinaryOp,
           typename T = typename std::decay<T1>::type>
  future<T> allreduce(T1 &&value, BinaryOp op) {
    intrank_t rank_me = upcxx::rank_me();
    intrank_t rank_n = upcxx::rank_n();
    
    // Given the parent function which flips the least significant one-bit
    //   parent(r) = r & (r-1)
    // Count number of bitpatterns in [0,rank_n) which when asked for
    // their parent, would produce us (rank_me).
    int incoming = 0;
    while(true) {
      intrank_t child = rank_me | (intrank_t(1)<<incoming);
      if(child == rank_me || rank_n <= child)
        break;
      incoming += 1;
    }
    incoming += 1; // add one for this rank
    
    auto *state = new dist_object<detail::allreduce_state<T,BinaryOp>>(
      detail::allreduce_state<T,BinaryOp>{
        incoming,
        std::move(op),
        std::forward<T1>(value),
        promise<T>{}
      }
    );
    
    future<T> result = (*state)->answer.get_future();
    
    // Initializing state->accum is our contribution. This could delete
    // `state` before it returns.
    (*state)->contributed(*state);
    
    return result;
  }
  
  namespace detail {
    template<typename T, typename Op>
    void allreduce_state<T,Op>::contributed(
        dist_object<allreduce_state> &this_obj
      ) {
      
      if(0 == --this->incoming) {
        intrank_t rank_me = upcxx::rank_me();
        intrank_t rank_n = upcxx::rank_n();
        
        if(rank_me == 0)
          this->broadcast(this_obj, this->accum, rank_n);
        else {
          // Our parent rank's index is the same as ours except with
          // the least significant one-bit set to zero.
          intrank_t parent = rank_me & (rank_me-1);
          
          rpc_ff(parent,
            [](dist_object<allreduce_state> &this_obj, T const &value) {
              this_obj->accum = this_obj->op(this_obj->accum, value);
              this_obj->contributed(this_obj);
            },
            this_obj, this->accum
          );
        }
      }
    }
    
    template<typename T, typename Op>
    void allreduce_state<T,Op>::broadcast(
        dist_object<allreduce_state> &this_obj,
        T const &value,
        intrank_t rank_ub
      ) {
      
      intrank_t rank_me = upcxx::rank_me();
      
      // binomial broadcast
      while(true) {
        intrank_t mid = rank_me + (rank_ub-rank_me)/2;
        
        if(mid == rank_me)
          break;
        
        rpc_ff(mid,
          [=](dist_object<allreduce_state> &this_obj, T const &value) {
            this_obj->broadcast(this_obj, value, rank_ub);
          },
          this_obj, value
        );
        
        rank_ub = mid;
      }
      
      this->answer.fulfill_result(value);
      
      delete &this_obj;
    }
  }
}
#endif
