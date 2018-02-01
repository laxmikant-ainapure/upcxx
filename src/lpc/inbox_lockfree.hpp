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
  
  template<int queue_n>
  struct lpc_inbox_lockfree: detail::lpc_inbox_lockfree_base {
    std::atomic<lpc*> head_[queue_n];
    std::atomic<std::atomic<lpc*>*> tailp_[queue_n];
  
  public:
    lpc_inbox_lockfree();
    ~lpc_inbox_lockfree();
    lpc_inbox_lockfree(lpc_inbox_lockfree const&) = delete;
    
    template<typename Fn1>
    void send(int q, Fn1 &&fn);
    
    // returns num lpc's executed
    int burst(int q, int burst_n = 100);
  };
  
  //////////////////////////////////////////////////////////////////////
  
  template<int queue_n>
  lpc_inbox_lockfree<queue_n>::lpc_inbox_lockfree() {
    for(int q=0; q < queue_n; q++) {
      head_[q].store(nullptr, std::memory_order_relaxed);
      tailp_[q].store(&head_[q], std::memory_order_relaxed);
    }
  }
  
  template<int queue_n>
  lpc_inbox_lockfree<queue_n>::~lpc_inbox_lockfree() {
    for(int q=0; q < queue_n; q++) {
      UPCXX_ASSERT(
        this->head_[q].load(std::memory_order_relaxed) == nullptr,
        "Abandoned lpc's detected."
      );
    }
  }
  
  template<int queue_n>
  template<typename Fn1>
  void lpc_inbox_lockfree<queue_n>::send(int q, Fn1 &&fn) {
    using Fn = typename std::decay<Fn1>::type;
    
    auto *m = new lpc_inbox_lockfree::lpc_impl<Fn>{std::forward<Fn1>(fn)};
    m->next.store(nullptr, std::memory_order_relaxed);
    
    std::atomic<lpc*> *got = this->tailp_[q].exchange(&m->next);
    got->store(m, std::memory_order_relaxed);
  }
}
#endif
