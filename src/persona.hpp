#ifndef _850ece2c_7b55_43a8_9e57_8cbd44974055
#define _850ece2c_7b55_43a8_9e57_8cbd44974055

#include <upcxx/backend_fwd.hpp>
#include <upcxx/future.hpp>
#include <upcxx/lpc_inbox.hpp>

namespace upcxx {
  class persona;
  class persona_scope;
  
  namespace detail {
    extern thread_local int tl_progressing/* = -1*/;
    extern thread_local persona *tl_top_persona/* = &tl_default_persona*/;
    extern thread_local persona tl_default_persona;
    extern thread_local persona_scope tl_default_persona_scope/*{tl_default_persona}*/;
    
    inline void* thread_id() {
      return (void*)&tl_default_persona;
    }
    
    // Enqueue a lambda onto persona's progress level queue. Note this
    // lambda may execute in the calling context if permitted.
    template<bool known_active, typename Fn>
    void persona_during(persona&, progress_level level, Fn &&fn);
    
    // Call `fn` on each `persona&` active with calling thread.
    template<typename Fn>
    void persona_foreach_active(Fn &&fn);
    
    // Executes `fn` with persona as top-most. Persona must be active
    // with calling thread.
    template<typename Fn>
    void persona_as_top(persona&, Fn &&fn);
    
    // Returns number of callbacks fired. Persona must be top-most active
    // on this thread.
    int persona_burst(persona&, progress_level level);
  }
  
  //////////////////////////////////////////////////////////////////////
  
  class persona {
    friend class persona_scope;
    
    template<bool known_active, typename Fn>
    friend void detail::persona_during(persona&, progress_level, Fn &&fn);
    
    template<typename Fn>
    friend void detail::persona_foreach_active(Fn&&);
    
    template<typename Fn>
    friend void detail::persona_as_top(persona&, Fn&&);
    
    friend int detail::persona_burst(persona&, progress_level);
    
    friend persona_scope& top_persona_scope();
    
  private:
    void *owner_id_;
    persona *next_;
    persona_scope *scope_;
    bool burstable_[/*progress_level's*/2];
    lpc_inbox</*queue_n=*/2, /*thread_safe=*/false> self_inbox_;
    lpc_inbox</*queue_n=*/2, /*thread_safe=*/true> peer_inbox_;
  
  public:
    backend::persona_state backend_state_;
    
  public:
    persona() {
      this->owner_id_ = nullptr;
      this->next_ = this; // not in a stack
      this->scope_ = nullptr;
      this->burstable_[(int)progress_level::internal] = true;
      this->burstable_[(int)progress_level::user] = true;
    }
    
    bool active_with_caller() const {
      return owner_id_ == detail::thread_id();
    }
    
  public:
    template<typename Fn>
    void lpc_ff(Fn fn) {
      if(this->active_with_caller())
        self_inbox_.send((int)progress_level::user, std::move(fn));
      else
        peer_inbox_.send((int)progress_level::user, std::move(fn));
    }
  
  private:
    template<typename Results, typename Promise>
    struct lpc_initiator_finish {
      Results results_;
      Promise *pro_;
      
      void operator()() {
        pro_->fulfill_result(std::move(results_));
        delete pro_;
      }
    };
    
    template<typename Promise>
    struct lpc_recipient_executed {
      persona *initiator_;
      Promise *pro_;
      
      template<typename ...Args>
      void operator()(Args &&...args) {
        std::tuple<typename std::decay<Args>::type...> results{
          std::forward<Args>(args)...
        };
        
        initiator_->lpc_ff(
          lpc_initiator_finish<decltype(results), Promise>{
            std::move(results),
            pro_
          }
        );
      }
    };
    
    template<typename Fn, typename Promise>
    struct lpc_recipient_execute {
      persona *initiator_;
      Promise *pro_;
      Fn fn_;
      
      void operator()() {
        upcxx::apply_tupled_as_future(fn_, std::tuple<>{})
          .then(lpc_recipient_executed<Promise>{initiator_, pro_});
      }
    };
  
  public:
    template<typename Fn>
    auto lpc(Fn fn)
      -> typename detail::future_from_tuple_t<
        detail::future_kind_shref<detail::future_header_ops_result>, // the default future kind
        typename decltype(
          upcxx::apply_tupled_as_future(fn, std::tuple<>{})
        )::results_type
      > {
      
      using results_type = typename decltype(
          upcxx::apply_tupled_as_future(fn, std::tuple<>{})
        )::results_type;
      
      promise<results_type> *pro = new promise<results_type>;
      auto ans = pro->get_future();
      
      this->lpc_ff(
        lpc_recipient_execute<Fn, promise<results_type>>{
          /*initiator*/&detail::tl_top_persona->peer_inbox_,
          /*promise*/pro,
          /*fn*/std::move(fn)
        }
      );
        
      return ans;
    }
  };
  
  //////////////////////////////////////////////////////////////////////
  
  class persona_scope {
    persona *persona_;
    void *lock_;
    void(*unlocker_)(void*);
  
  public:
    persona_scope(persona &p):
      persona_{&p},
      lock_{nullptr},
      unlocker_{nullptr} {
      
      UPCXX_ASSERT(p.next_ == &p, "Persona already in some thread's stack.");
      UPCXX_ASSERT(detail::tl_progressing < 0, "Cannot change active persona stack while in progress.");
      
      p.owner_id_ = detail::thread_id();
      p.next_ = detail::tl_top_persona;
      detail::tl_top_persona = &p;
      p.scope_ = this;
    }
    
    template<typename Mutex>
    persona_scope(Mutex &lock, persona &p) {
      this->persona_ = &p;
      this->lock_ = &lock;
      this->unlocker_ = (void(*)(void*))[](void *lock) {
        static_cast<Mutex*>(lock)->unlock();
      };
      
      lock.lock();

      UPCXX_ASSERT(p.next_ == &p, "Persona already active with some thread.");
      UPCXX_ASSERT(detail::tl_progressing < 0, "Cannot change active persona stack while in progress.");
      
      p.owner_id_ = detail::thread_id();
      p.next_ = detail::tl_top_persona;
      detail::tl_top_persona = &p;
      p.scope_ = this;
    }
    
    persona_scope(persona_scope &&that) {
      UPCXX_ASSERT(detail::tl_progressing < 0, "Cannot change active persona stack while in progress.");
      
      persona_ = that.persona_;
      lock_ = that.lock_;
      unlocker_ = that.unlocker_;
      
      persona_->scope_ = this;
      
      that.persona_ = nullptr;
      that.lock_ = nullptr;
      that.unlocker_ = nullptr;
    }
    
    ~persona_scope() {
      if(persona_ != nullptr) {
        UPCXX_ASSERT(persona_ == detail::tl_top_persona);
        UPCXX_ASSERT(detail::tl_progressing < 0, "Cannot change active persona stack while in progress.");
        
        detail::tl_top_persona = persona_->next_;
        persona_->next_ = persona_;
        persona_->owner_id_ = nullptr;
        persona_->scope_ = nullptr;
        
        if(unlocker_)
          unlocker_(lock_);
      }
    }
  };
  
  //////////////////////////////////////////////////////////////////////
  
  inline persona& default_persona() {
    return detail::tl_default_persona;
  }
  
  inline persona& current_persona() {
    return *detail::tl_top_persona;
  }
  
  inline persona_scope& top_persona_scope() {
    return *detail::tl_top_persona->scope_;
  }
  
  template<bool known_active, typename Fn>
  void detail::persona_during(persona &p, progress_level level, Fn &&fn) {
    if(known_active || p.active_with_caller()) {
      if(level == progress_level::internal || (
          (int)level <= tl_progressing && p.burstable_[(int)level]
        )) {
        p.burstable_[(int)level] = false;
        persona *top = tl_top_persona;
        tl_top_persona = &p;
        
        fn();
        
        p.burstable_[(int)level] = true;
        tl_top_persona = top;
      }
      else
        p.self_inbox_.send((int)level, std::forward<Fn>(fn));
    }
    else
      p.peer_inbox_.send((int)level, std::forward<Fn>(fn));
  }

  template<typename Fn>
  void detail::persona_foreach_active(Fn &&body) {
    persona *top = tl_top_persona;
    persona *p = top;
    while(p != nullptr) {
      tl_top_persona = p;
      body(*p);
      p = p->next_;
    }
    tl_top_persona = top;
  }
  
  template<typename Fn>
  void detail::persona_as_top(persona &p, Fn &&fn) {
    persona *top = tl_top_persona;
    tl_top_persona = &p;
    fn();
    tl_top_persona = top;
  }
  
  inline int detail::persona_burst(persona &p, upcxx::progress_level level) {
    constexpr int q_internal = (int)progress_level::internal;
    constexpr int q_user     = (int)progress_level::user;
    
    int exec_n = 0;
    
    UPCXX_ASSERT(p.burstable_[q_user]); // If this failed then an internal action is trying to burst internal progress.
    if(p.burstable_[q_user]) {
      p.burstable_[q_internal] = false;
      exec_n += p.peer_inbox_.burst(q_internal);
      exec_n += p.self_inbox_.burst(q_internal);
      p.burstable_[q_internal] = true;
    }
    
    if(level == progress_level::user) {
      UPCXX_ASSERT(p.burstable_[q_user]);
      p.burstable_[q_user] = false;
      exec_n += p.peer_inbox_.burst(q_user);
      exec_n += p.self_inbox_.burst(q_user);
      p.burstable_[q_user] = true;
    }
    
    return exec_n;
  }
}
#endif
