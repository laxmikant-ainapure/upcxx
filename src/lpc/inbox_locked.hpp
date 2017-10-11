#ifndef _67b46dbc_0075_4d4d_9e16_68ca3b7a80ff
#define _67b46dbc_0075_4d4d_9e16_68ca3b7a80ff

#include <upcxx/diagnostic.hpp>

#include <mutex>

namespace upcxx {
  namespace detail {
    struct lpc_inbox_locked_base {
      struct lpc {
        lpc *next;
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
  struct lpc_inbox_locked: detail::lpc_inbox_locked_base {
    lpc *head_[queue_n];
    lpc **tailp_[queue_n];
    std::mutex lock_;
    
  public:
    lpc_inbox_locked();
    ~lpc_inbox_locked();
    lpc_inbox_locked(lpc_inbox_locked const&) = delete;
    
    template<typename Fn1>
    void send(int q, Fn1 &&fn);
    
    // returns num lpc's executed
    int burst(int q, int burst_n = 100);
  };
  
  //////////////////////////////////////////////////////////////////////
  
  template<int queue_n>
  lpc_inbox_locked<queue_n>::lpc_inbox_locked() {
    for(int q=0; q < queue_n; q++) {
      head_[q] = nullptr;
      tailp_[q] = &head_[q];
    }
  }
  
  template<int queue_n>
  lpc_inbox_locked<queue_n>::~lpc_inbox_locked() {
    for(int q=0; q < queue_n; q++) {
      UPCXX_ASSERT(this->head_[q] == nullptr, "Abandoned lpc's detected.");
    }
  }
  
  template<int queue_n>
  template<typename Fn1>
  void lpc_inbox_locked<queue_n>::send(int q, Fn1 &&fn) {
    using Fn = typename std::decay<Fn1>::type;
    
    auto *m = new lpc_inbox_locked::lpc_impl<Fn>{std::forward<Fn1>(fn)};
    m->next = nullptr;

    { std::lock_guard<std::mutex> locked{this->lock_};
    
      *this->tailp_[q] = m;
      this->tailp_[q] = &m->next;
    }
  }
}
#endif
