#ifndef _b68cb319_851a_4f86_bf16_a95c9b16a93f
#define _b68cb319_851a_4f86_bf16_a95c9b16a93f

#include <upcxx/diagnostic.hpp>

namespace upcxx {
  namespace detail {
    struct lpc_inbox_syncfree_base {
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
  struct lpc_inbox_syncfree: detail::lpc_inbox_syncfree_base {
    lpc *head_[queue_n];
    // lpc **tailp_[queue_n] = {&this->head_[q]...};
    std::uintptr_t tailp_xor_head_[queue_n];
  
  private:
    lpc** get_tailp(int q) {
      return reinterpret_cast<lpc**>(tailp_xor_head_[q] ^ reinterpret_cast<std::uintptr_t>(&head_[q]));
    }
    void set_tailp(int q, lpc **val) {
      tailp_xor_head_[q] = reinterpret_cast<std::uintptr_t>(val) ^ reinterpret_cast<std::uintptr_t>(&head_[q]);
    }
  
  public:
    constexpr lpc_inbox_syncfree():
      head_(),
      tailp_xor_head_() {
    }
    lpc_inbox_syncfree(lpc_inbox_syncfree const&) = delete;
    
    template<typename Fn1>
    void send(int q, Fn1 &&fn);
    
    // returns num lpc's executed
    int burst(int q, int burst_n = 100);
  };
  
  //////////////////////////////////////////////////////////////////////
  
  template<int queue_n>
  template<typename Fn1>
  void lpc_inbox_syncfree<queue_n>::send(int q, Fn1 &&fn) {
    using Fn = typename std::decay<Fn1>::type;
    
    auto *m = new lpc_inbox_syncfree::lpc_impl<Fn>{std::forward<Fn1>(fn)};
    m->next = nullptr;
    *this->get_tailp(q) = m;
    this->set_tailp(q, &m->next);
  }
}
#endif
