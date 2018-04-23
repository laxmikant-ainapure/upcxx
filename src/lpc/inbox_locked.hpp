#ifndef _67b46dbc_0075_4d4d_9e16_68ca3b7a80ff
#define _67b46dbc_0075_4d4d_9e16_68ca3b7a80ff

#include <upcxx/diagnostic.hpp>

#include <atomic>
#include <mutex>

namespace upcxx {
  namespace detail {
    struct lpc_inbox_locked_base {
      static constexpr int lock_log2_n = 9;
      static std::mutex the_locks[1<<lock_log2_n];
      
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
  
  // This type is contained within `__thread` storage, so it must be:
  //   1. trivially destructible.
  //   2. constexpr constructible equivalent to zero-initialization.
  template<int queue_n>
  struct lpc_inbox_locked: detail::lpc_inbox_locked_base {
    lpc *head_[queue_n];
    lpc *tail_[queue_n];
    
  private:
    std::mutex& get_lock() {
      constexpr std::uintptr_t magic = std::uintptr_t(
          8*sizeof(std::uintptr_t) == 32
            ? 0x9e3779b9u
            : 0x9e3779b97f4a7c15u
        );
      std::uintptr_t u = reinterpret_cast<std::uintptr_t>(this);
      u ^= u >> 8*sizeof(std::uintptr_t)/2;
      u *= magic;
      u >>= 8*sizeof(std::uintptr_t) - lock_log2_n;
      return the_locks[u];
    }
    
  public:
    constexpr lpc_inbox_locked():
      head_(),
      tail_() {
    }
    lpc_inbox_locked(lpc_inbox_locked const&) = delete;
    
    template<typename Fn1>
    void send(int q, Fn1 &&fn);
    
    // returns num lpc's executed
    int burst(int q, int burst_n = 100);
  };
  
  //////////////////////////////////////////////////////////////////////
  
  template<int queue_n>
  template<typename Fn1>
  void lpc_inbox_locked<queue_n>::send(int q, Fn1 &&fn) {
    using Fn = typename std::decay<Fn1>::type;
    
    auto *m = new lpc_inbox_locked::lpc_impl<Fn>{std::forward<Fn1>(fn)};
    m->next = nullptr;

    {
      std::lock_guard<std::mutex> locked(this->get_lock());
    
      if(this->tail_[q] != nullptr)
        this->tail_[q]->next = m;
      else
        this->head_[q] = m;
      
      this->tail_[q] = m;
    }
  }
}
#endif
