#ifndef _2d897dc6_1696_4bd9_b530_6d923356fa84
#define _2d897dc6_1696_4bd9_b530_6d923356fa84

#include <upcxx/diagnostic.hpp>

#include <atomic>
#include <mutex>

namespace upcxx {
  template<int queue_n>
  struct lpc_inbox_base1 {
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

    lpc *head_[queue_n];
    lpc **tailp_[queue_n];
    
    lpc_inbox_base1() {
      for(int q=0; q < queue_n; q++) {
        head_[q] = nullptr;
        tailp_[q] = &head_[q];
      }
    }
  };
  
  template<int queue_n, bool thread_safe>
  struct lpc_inbox_base;
  
  template<int queue_n>
  struct lpc_inbox_base<queue_n, /*thread_safe=*/false>:
    lpc_inbox_base1<queue_n> {
    
    struct lock_guard {
      ~lock_guard() {}
    };
    
    lock_guard locked_scope() {
      return lock_guard{};
    }
  };
  
  template<int queue_n>
  struct lpc_inbox_base<queue_n, /*thread_safe=*/true>:
    lpc_inbox_base1<queue_n> {
    
    std::mutex lock_;
    
    struct lock_guard {
      std::mutex *m;
      
      lock_guard(std::mutex &m):
        m{&m} {
        m.lock();
      }
      lock_guard(lock_guard &&that) {
        this->m = that.m;
        that.m = nullptr;
      }
      ~lock_guard() {
        if(m != nullptr)
          m->unlock();
      }
    };
    
    lock_guard locked_scope() {
      return lock_guard{lock_};
    }
  };
  
  template<int queue_n, bool thread_safe>
  class lpc_inbox: lpc_inbox_base<queue_n, thread_safe> {
  public:
    lpc_inbox() = default;
    ~lpc_inbox();
    lpc_inbox(lpc_inbox const&) = delete;
    lpc_inbox(lpc_inbox &&that);
    
    template<typename Fn1>
    void send(int q, Fn1 &&fn);
    
    // returns num lpc's executed
    int burst(int q, int burst_n = 100);
  };
  
  //////////////////////////////////////////////////////////////////////
  
  template<int queue_n, bool thread_safe>
  lpc_inbox<queue_n, thread_safe>::~lpc_inbox() {
    auto locked = this->locked_scope();
    for(int q=0; q < queue_n; q++)
      UPCXX_ASSERT(this->head_[q] == nullptr, "Abandoned lpc's detected.");
  }
  
  template<int queue_n, bool thread_safe>
  lpc_inbox<queue_n, thread_safe>::lpc_inbox(lpc_inbox &&that) {
    auto locked = this->locked_scope();
    
    for(int q=0; q < queue_n; q++) {
      // take list from `that`
      this->head_[q] = that.head_[q];
      this->tailp_[q] =
        that.tailp_[q] == &that.head_[q]
          ? &this->head_[q]
          : that.tailp_[q];
      
      // give `that` empty list
      that.head_[q] = nullptr;
      that.tailp_[q] = &that.head_[q];
    }
  }
  
  template<int queue_n, bool thread_safe>
  template<typename Fn1>
  void lpc_inbox<queue_n, thread_safe>::send(int q, Fn1 &&fn) {
    using Fn = typename std::decay<Fn1>::type;
    
    auto *m = new typename lpc_inbox_base1<queue_n>::template lpc_impl<Fn>{
      std::forward<Fn1>(fn)
    };
    m->next = nullptr;
    
    { // link message into list
      auto locked = this->locked_scope();
      *this->tailp_[q] = m;
      this->tailp_[q] = &m->next;
    }
  }
}
#endif
