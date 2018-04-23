#ifndef _59d24b21_7503_4797_8c25_030246946671
#define _59d24b21_7503_4797_8c25_030246946671

#include <upcxx/diagnostic.hpp>

#include <atomic>

namespace upcxx {
  namespace detail {
    struct lpc_inbox_lockfree_base {
      struct lpc {
        std::atomic<lpc*> next;
        virtual void execute_and_delete() = 0;
      };
      
      template<typename Fn>
      struct lpc_impl final: lpc {
        Fn fn_;
        lpc_impl(Fn fn): fn_{std::move(fn)} {}
        
        void execute_and_delete() {
          this->fn_();
          delete this;
        }
      };
    };
  }
  
  // This type is contained within `__thread` storage, so it must be:
  //   1. trivially destructible.
  //   2. constexpr constructible equivalent to zero-initialization.
  template<int queue_n>
  struct lpc_inbox_lockfree: detail::lpc_inbox_lockfree_base {
    std::atomic<lpc*> head_[queue_n];
    //std::atomic<std::atomic<lpc*>*> tailp_[queue_n] = {&this->head_[q]...};
    std::atomic<std::uintptr_t> tailp_xor_head_[queue_n];
    
  private:
    constexpr std::atomic<lpc*>* decode_tailp(int q, std::uintptr_t u) const {
      return reinterpret_cast<std::atomic<lpc*>*>(
        u ^ reinterpret_cast<std::uintptr_t>(&head_[q])
      );
    }
    constexpr std::uintptr_t encode_tailp(int q, std::atomic<lpc*> *val) const {
      return reinterpret_cast<std::uintptr_t>(val) ^ reinterpret_cast<std::uintptr_t>(&head_[q]);
    }
  
  public:
    constexpr lpc_inbox_lockfree():
      head_(),
      tailp_xor_head_() {
    }
    lpc_inbox_lockfree(lpc_inbox_lockfree const&) = delete;
    
    template<typename Fn1>
    void send(int q, Fn1 &&fn);
    
    // returns num lpc's executed
    int burst(int q, int burst_n = 100);
  };
  
  //////////////////////////////////////////////////////////////////////
  
  template<int queue_n>
  template<typename Fn1>
  void lpc_inbox_lockfree<queue_n>::send(int q, Fn1 &&fn) {
    using Fn = typename std::decay<Fn1>::type;
    
    auto *m = new lpc_inbox_lockfree::lpc_impl<Fn>{std::forward<Fn1>(fn)};
    m->next.store(nullptr, std::memory_order_relaxed);
    
    std::atomic<lpc*> *got = decode_tailp(q,
                               this->tailp_xor_head_[q].exchange(
                                 encode_tailp(q, &m->next)
                               )
                             );
    got->store(m, std::memory_order_relaxed);
  }
}
#endif
