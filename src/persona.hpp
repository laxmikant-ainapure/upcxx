#ifndef _850ece2c_7b55_43a8_9e57_8cbd44974055
#define _850ece2c_7b55_43a8_9e57_8cbd44974055

#include <upcxx/backend_fwd.hpp>
#include <upcxx/future.hpp>
#include <upcxx/lpc/inbox.hpp>

#include <atomic>
#include <cstdint>
#include <type_traits>

namespace upcxx {
  class persona;
  class persona_scope;
  namespace detail {
    struct persona_scope_raw;
    struct persona_scope_redundant;
    struct persona_tls;
  }
  
  // This type is contained within `__thread` storage, so it must be:
  //   1. trivially destructible.
  //   2. constexpr constructible equivalent to zero-initialization.
  class persona {
    friend struct detail::persona_tls;
    friend class persona_scope;
    friend struct detail::persona_scope_redundant;
    
  private:
    // persona *owner = this;
    std::atomic<std::uintptr_t> owner_xor_this_;
    
    // bool burstable[2] = {true, true};
    bool not_burstable_[/*progress_level's*/2];
    
    lpc_inbox</*queue_n=*/2, /*thread_safe=*/false> self_inbox_;
    lpc_inbox</*queue_n=*/2, /*thread_safe=*/true> peer_inbox_;
  
  public:
    backend::persona_state backend_state_;
  
  private:
    persona* get_owner() const;
    void set_owner(persona *val);
    
  private:
    // Constructs the default persona for the current thread.
    constexpr persona(detail::internal_only):
      owner_xor_this_(), // owner = this, default persona's are their own owner
      not_burstable_(),
      self_inbox_(),
      peer_inbox_(),
      backend_state_() {
    }
  
  public:
    // Constructs a non-default persona.
    persona():
      owner_xor_this_(reinterpret_cast<std::uintptr_t>(this)), // owner = null
      not_burstable_(),
      self_inbox_(),
      peer_inbox_(),
      backend_state_() {
    }
    
    bool active_with_caller() const;
    bool active_with_caller(detail::persona_tls &tls) const;
    bool active() const;
    
  public:
    template<typename Fn>
    void lpc_ff(detail::persona_tls &tls, Fn fn);
    
    template<typename Fn>
    void lpc_ff(Fn fn);
  
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
        upcxx::apply_as_future(fn_)
          .then(lpc_recipient_executed<Promise>{initiator_, pro_});
      }
    };
  
  public:
    template<typename Fn>
    auto lpc(Fn fn)
      -> typename detail::future_from_tuple_t<
        detail::future_kind_shref<detail::future_header_ops_general>, // the default future kind
        typename decltype(upcxx::apply_as_future(fn))::results_type
      >;
  };
  
  //////////////////////////////////////////////////////////////////////////////
  // persona_scope
  
  namespace detail {
    // Holds all the fields for a persona_scope but in a trivial type.
    struct persona_scope_raw {
      friend struct detail::persona_tls;
      
      persona_scope_raw *next_;
      std::uintptr_t persona_xor_default_;
      persona_scope *next_unique_;
      union {
        std::uintptr_t restore_active_bloom_; // used by `persona_scope`
        detail::persona_tls *tls; // used by `persona_scope_redundnat`
      };
      union {
        void *lock_; // used by `persona_scope`
        persona *restore_top_persona_; // used by `persona_scope_redundnat`
      };
      void(*unlocker_)(void*);
      
      // Constructs the default persona scope for the current thread.
      constexpr persona_scope_raw():
        next_(),
        persona_xor_default_(),
        next_unique_(),
        restore_active_bloom_(),
        lock_(),
        unlocker_() {
      }
      
      //////////////////////////////////////////////////////////////////////////
      // accessors
      
      persona* get_persona(detail::persona_tls &tls) const;
      void set_persona(persona *val, detail::persona_tls &tls);
    };
    
    // A more efficient kind of persona scope used when its known that we're
    // redundantly pushing a persona to the current thread's stack when its
    // already a member of the stack.
    struct persona_scope_redundant: persona_scope_raw {
      persona_scope_redundant(persona &persona, detail::persona_tls &tls);
      ~persona_scope_redundant();
    };
  }
  
  class persona_scope: public detail::persona_scope_raw {
    friend struct detail::persona_tls;
    
  private:
    constexpr persona_scope() = default;
    
    persona_scope(persona &persona, detail::persona_tls &tls);
    
    template<typename Mutex>
    persona_scope(Mutex &lock, persona &persona, detail::persona_tls &tls);
    
  public:
    persona_scope(persona &persona);
    
    template<typename Mutex>
    persona_scope(Mutex &lock, persona &persona);
    
    persona_scope(persona_scope const&) = delete;
    
    persona_scope(persona_scope &&that) {
      *static_cast<detail::persona_scope_raw*>(this) = static_cast<detail::persona_scope_raw&>(that);
      that.next_ = reinterpret_cast<persona_scope*>(0x1);
    }
    
    ~persona_scope();
  };
  
  //////////////////////////////////////////////////////////////////////
  // detail::persona_tls
  
  namespace detail {
    // This type is contained within `__thread` storage, so it must be:
    //   1. trivially destructible.
    //   2. constexpr constructible equivalent to zero-initialization.
    struct persona_tls {
      // int progressing = -1;
      int progressing_plus1;
      std::uintptr_t active_bloom;
      persona default_persona;
      // persona_scope default_scope;
      persona_scope_raw default_scope_raw;
      // persona_scope *top = &this->default_scope;
      std::uintptr_t top_xor_default; // = xor(top, &this->default_scope)
      // persona_scope *top_unique = &this->default_scope;
      std::uintptr_t top_unique_xor_default;  // = xor(top_unique, &this->default_scope)
      // persona *top_persona = &this->default_persona;
      std::uintptr_t top_persona_xor_default;  // = xor(top_persona, &this->default_persona)
      
      static_assert(std::is_trivially_destructible<persona>::value, "upcxx::persona must be TriviallyDestructible.");
      static_assert(std::is_trivially_destructible<persona_scope_raw>::value, "upcxx::detail::persona_scope_raw must be TriviallyDestructible.");
      
      constexpr persona_tls():
        progressing_plus1(),
        active_bloom(),
        default_persona(internal_only()), // call special constructor that builds default persona
        default_scope_raw(),
        top_xor_default(),
        top_unique_xor_default(),
        top_persona_xor_default() {
      }
      
      //////////////////////////////////////////////////////////////////////////
      // getters/setters for fields with zero-friendly encodings
      
      persona_scope& default_scope() {
        return *static_cast<persona_scope*>(&default_scope_raw);
      }
      persona_scope const& default_scope() const {
        return *static_cast<persona_scope const*>(&default_scope_raw);
      }
      
      int get_progressing() const { return progressing_plus1 - 1; }
      void set_progressing(int val) { progressing_plus1 = val + 1; }
      
      persona_scope_raw* get_top_scope() const {
        return reinterpret_cast<persona_scope_raw*>(
          top_xor_default ^ reinterpret_cast<std::uintptr_t>(&default_scope())
        );
      }
      void set_top_scope(persona_scope_raw *val) {
        top_xor_default = reinterpret_cast<std::uintptr_t>(&default_scope())
                        ^ reinterpret_cast<std::uintptr_t>(val);
      }
      
      persona_scope* get_top_unique_scope() const {
        return reinterpret_cast<persona_scope*>(
          top_unique_xor_default ^ reinterpret_cast<std::uintptr_t>(&default_scope())
        );
      }
      void set_top_unique_scope(persona_scope *val) {
        top_unique_xor_default = reinterpret_cast<std::uintptr_t>(&default_scope())
                               ^ reinterpret_cast<std::uintptr_t>(val);
      }
      
      persona* get_top_persona() const {
        return reinterpret_cast<persona*>(
          top_persona_xor_default ^ reinterpret_cast<std::uintptr_t>(&default_persona)
        );
      }
      void set_top_persona(persona *val) {
        top_persona_xor_default = reinterpret_cast<std::uintptr_t>(&default_persona)
                                ^ reinterpret_cast<std::uintptr_t>(val);
      }
      
      //////////////////////////////////////////////////////////////////////////
      // bloom filter for active set of personas with this thread
      
      // Compute query mask for given pointer. If all the 1-bits in a query mask
      // are set in the filter, then it reports membership in the set as true.
      std::uintptr_t bloom_query(persona const *x) const;
      
      bool possibly_active(persona const *x) const {
        std::uintptr_t q = bloom_query(x);
        return (this->active_bloom & q) == q;
      }
      
      void include_active(persona const *x) {
        this->active_bloom |= bloom_query(x);
      }
      
      //////////////////////////////////////////////////////////////////////////
      // operations
      
      // Enqueue a lambda onto persona's progress level queue. Note: this
      // lambda may execute in the calling context if permitted.
      template<typename Fn, bool known_active>
      void during(persona&, progress_level level, Fn &&fn, std::integral_constant<bool,known_active> known_active1 = {});
      
      // Enqueue a lambda onto persona's progress level queue. Unlike
      // `during`, lambda will definitely not execute in calling context.
      template<typename Fn, bool known_active=false>
      void defer(persona&, progress_level level, Fn &&fn, std::integral_constant<bool,known_active> known_active1 = {});
      
      // Call `fn` on each `persona&` active with calling thread.
      template<typename Fn>
      void foreach_active_as_top(Fn &&fn);
      
      // Returns number of lpc's fired. Persona *should* be top-most active
      // on this thread, but don't think anything would break if it isn't.
      int burst(persona&, progress_level level);
    };
    
    extern __thread persona_tls the_persona_tls;
    
    inline void* thread_id() {
      return (void*)&the_persona_tls;
    }
  }
  
  //////////////////////////////////////////////////////////////////////////////
  // persona definitions
  
  inline persona* persona::get_owner() const {
    return reinterpret_cast<persona*>(
      owner_xor_this_.load(std::memory_order_relaxed) ^
      reinterpret_cast<std::uintptr_t>(this)
    );
  }
  
  inline void persona::set_owner(persona *val) {
    owner_xor_this_.store(
      reinterpret_cast<std::uintptr_t>(val) ^ reinterpret_cast<std::uintptr_t>(this),
      std::memory_order_relaxed
    );
  }
  
  inline bool persona::active_with_caller() const {
    return active_with_caller(detail::the_persona_tls);
  }
  
  inline bool persona::active_with_caller(detail::persona_tls &tls) const {
    if(!tls.possibly_active(this))
      return false;
    
    // The bloom filter query above filters out most accesses to the owner field
    // of personas not active with this thread, since that might involve
    // cache-line traffic. We use a soft fence here to encourage the compiler
    // not to issue the load unless the bloom filter fails.
    std::atomic_signal_fence(std::memory_order_acq_rel);
    
    return this->get_owner() == &tls.default_persona;
  }
  
  inline bool persona::active() const {
    return this->get_owner() != nullptr;
  }

  template<typename Fn>
  void persona::lpc_ff(Fn fn) {
    this->lpc_ff(detail::the_persona_tls, std::forward<Fn>(fn));
  }
  
  template<typename Fn>
  void persona::lpc_ff(detail::persona_tls &tls, Fn fn) {
    if(this->active_with_caller(tls))
      this->self_inbox_.send((int)progress_level::user, std::move(fn));
    else
      this->peer_inbox_.send((int)progress_level::user, std::move(fn));
  }
  
  template<typename Fn>
  auto persona::lpc(Fn fn)
    -> typename detail::future_from_tuple_t<
      detail::future_kind_shref<detail::future_header_ops_general>, // the default future kind
      typename decltype(upcxx::apply_as_future(fn))::results_type
    > {
    
    using results_type = typename decltype(upcxx::apply_as_future(fn))::results_type;
    using results_promise = upcxx::tuple_types_into_t<results_type, promise>;
    
    detail::persona_tls &tls = detail::the_persona_tls;
    
    results_promise *pro = new results_promise;
    auto ans = pro->get_future();
    
    this->lpc_ff(tls,
      lpc_recipient_execute<Fn, results_promise>{
        /*initiator*/tls.get_top_persona(),
        /*promise*/pro,
        /*fn*/std::move(fn)
      }
    );
      
    return ans;
  }
  
  //////////////////////////////////////////////////////////////////////
  // persona_scope definitions
    
  inline persona* detail::persona_scope_raw::get_persona(detail::persona_tls &tls) const {
    return reinterpret_cast<persona*>(
      persona_xor_default_ ^
      reinterpret_cast<std::uintptr_t>(&tls.default_persona)
    );
  }
  
  inline void detail::persona_scope_raw::set_persona(persona *val, detail::persona_tls &tls) {
    persona_xor_default_ = reinterpret_cast<std::uintptr_t>(val)
                         ^ reinterpret_cast<std::uintptr_t>(&tls.default_persona);
  }
  
  inline detail::persona_scope_redundant::persona_scope_redundant(
      persona &persona,
      detail::persona_tls &tls
    ) {
    this->tls = &tls;
    
    // point this scope at persona
    this->set_persona(&persona, tls);
    
    // push this scope on this thread's stack
    this->next_ = tls.get_top_scope();
    tls.set_top_scope(this);
    this->restore_top_persona_ = tls.get_top_persona();
    tls.set_top_persona(&persona);
  }
  
  inline detail::persona_scope_redundant::~persona_scope_redundant() {
    detail::persona_tls &tls = *this->tls;
    UPCXX_ASSERT(this == tls.get_top_scope());
    tls.set_top_scope(this->next_);
    tls.set_top_persona(this->restore_top_persona_);
  }
  
  inline persona_scope::persona_scope(persona &persona):
    persona_scope(persona, detail::the_persona_tls) {
  }
  
  inline persona_scope::persona_scope(persona &p, detail::persona_tls &tls) {
    this->lock_ = nullptr;
    this->unlocker_ = nullptr;
    
    bool was_active = p.active();
    UPCXX_ASSERT(!was_active || p.active_with_caller(tls), "Persona already active in another thread.");
    
    // point this scope at persona
    this->set_persona(&p, tls);
    
    // set persona's owner thread to this thread
    p.set_owner(&tls.default_persona);
    
    // push this scope on this thread's stack
    this->next_ = tls.get_top_scope();
    tls.set_top_scope(this);
    tls.set_top_persona(&p);
    
    this->restore_active_bloom_ = tls.active_bloom;
    
    if(!was_active) {
      this->next_unique_ = tls.get_top_unique_scope();
      tls.set_top_unique_scope(this);
      
      // include persona in thread's active set filter
      tls.include_active(&p);
    }
    else
      this->next_unique_ = reinterpret_cast<persona_scope*>(0x1);
    
    UPCXX_ASSERT(p.active_with_caller(tls));
  }
  
  template<typename Mutex>
  persona_scope::persona_scope(Mutex &lock, persona &p):
    persona_scope(lock, p, detail::the_persona_tls) {
  }
  
  template<typename Mutex>
  persona_scope::persona_scope(Mutex &lock, persona &p, detail::persona_tls &tls) {
    this->lock_ = &lock;
    this->unlocker_ = (void(*)(void*))[](void *lock) {
      static_cast<Mutex*>(lock)->unlock();
    };
    
    lock.lock();
    
    bool was_active = p.active();
    UPCXX_ASSERT(!was_active || p.active_with_caller(tls), "Persona already active in another thread.");
    
    // point this scope at persona
    this->set_persona(&p, tls);
    
    // set persona's owner thread to this thread
    p.set_owner(&tls.default_persona);
    
    // push this scope on this thread's stack
    this->next_ = tls.get_top_scope();
    tls.set_top_scope(this);
    tls.set_top_persona(&p);
    
    this->restore_active_bloom_ = tls.active_bloom;
    
    if(!was_active) {
      this->next_unique_ = tls.get_top_unique_scope();
      tls.set_top_unique_scope(this);
      
      // include persona in thread's active set filter
      tls.include_active(&p);
    }
    else
      this->next_unique_ = reinterpret_cast<persona_scope*>(0x1);
    
    UPCXX_ASSERT(p.active_with_caller(tls));
  }
  
  inline persona_scope::~persona_scope() {
    if(this->next_ != reinterpret_cast<persona_scope*>(0x1)) {
      detail::persona_tls &tls = detail::the_persona_tls;
      
      UPCXX_ASSERT(this == tls.get_top_scope());
      
      tls.set_top_scope(this->next_);
      tls.set_top_persona(this->next_->get_persona(tls));
      this->get_persona(tls)->set_owner(nullptr);
      
      if(this->next_unique_ != reinterpret_cast<persona_scope*>(0x1))
        tls.set_top_unique_scope(this->next_unique_);
      
      tls.active_bloom = this->restore_active_bloom_;
      
      if(this->unlocker_)
        this->unlocker_(this->lock_);
    }
  }
  
  //////////////////////////////////////////////////////////////////////
  
  inline persona& default_persona() {
    detail::persona_tls &tls = detail::the_persona_tls;
    return tls.default_persona;
  }
  
  inline persona& current_persona() {
    detail::persona_tls &tls = detail::the_persona_tls;
    return *tls.get_top_scope()->get_persona(tls);
  }
  
  inline persona_scope& default_persona_scope() {
    detail::persona_tls &tls = detail::the_persona_tls;
    return tls.default_scope();
  }
  
  inline persona_scope& top_persona_scope() {
    detail::persona_tls &tls = detail::the_persona_tls;
    return *static_cast<persona_scope*>(tls.get_top_scope());
  }
  
  //////////////////////////////////////////////////////////////////////
  
  inline std::uintptr_t detail::persona_tls::bloom_query(persona const *x) const {
    constexpr int bits = 8*sizeof(std::uintptr_t);
    static_assert(bits == 32 || bits == 64, "Crazy architecture!");
    
    constexpr std::uintptr_t magic = std::uintptr_t(
        bits == 32 ? 0x9e3779b9u : 0x9e3779b97f4a7c15u
      );
    
    // XOR bits of `x` pointer against the address of this thread's default
    // persona. Importantly, this yields zero iff `x` is our default persona.
    std::uintptr_t u = reinterpret_cast<std::uintptr_t>(x);
    u ^= reinterpret_cast<std::uintptr_t>(&this->default_persona);
    
    // Now take `u` and mix up the bits twice using injective functions.
    std::uintptr_t a = u ^ (u >> bits/2);
    std::uintptr_t b = a * magic;
    
    // Ideally `a` and `b` would be "good" hashes of `u`, therefor you could
    // expect each to be a 50/50 split of 1 vs 0 bits. ANDing them together
    // would then produces a word with 25% 1 bits. The recommended number of
    // hash functions for a bloom filter with 64 bits and an expected population
    // of 3 is 14. So our mask `a & b` is like we used ~16 hash functions,
    // which is darn close to that ideal of 14.
    
    // Also, since `b = a * <odd number>`, its impossible for `a & b` to be zero
    // unless `a` is zero, and `a` could only be zero if `x` is the default
    // persona of this thread. A query mask of all zeros is dangerous since it
    // always reports true. But we've guaranteed that only this thread's
    // default persona can generate that mask, which is precisely the persona
    // that always should test as true!
    return a & b;
  }
  
  template<typename Fn, bool known_active>
  void detail::persona_tls::during(
      persona &p,
      progress_level level,
      Fn &&fn,
      std::integral_constant<bool, known_active>
    ) {
    persona_tls &tls = *this;
    
    if(known_active || p.active_with_caller(tls)) {
      if(level == progress_level::internal || (
          (int)level <= tls.get_progressing() && !p.not_burstable_[(int)level]
        )) {
        p.not_burstable_[(int)level] = true;
        {
          persona_scope_redundant tmp(p, tls);
          fn();
        }
        p.not_burstable_[(int)level] = false;
      }
      else
        p.self_inbox_.send((int)level, std::forward<Fn>(fn));
    }
    else
      p.peer_inbox_.send((int)level, std::forward<Fn>(fn));
  }

  template<typename Fn, bool known_active>
  void detail::persona_tls::defer(
      persona &p,
      progress_level level,
      Fn &&fn,
      std::integral_constant<bool, known_active>
    ) {
    persona_tls &tls = *this;
    
    if(known_active || p.active_with_caller(tls))
      p.self_inbox_.send((int)level, std::forward<Fn>(fn));
    else
      p.peer_inbox_.send((int)level, std::forward<Fn>(fn));
  }

  template<typename Fn>
  void detail::persona_tls::foreach_active_as_top(Fn &&body) {
    persona_tls &tls = *this;
    persona_scope *u = tls.get_top_unique_scope();
    do {
      persona *p = u->get_persona(tls);
      persona_scope_redundant tmp(*p, tls);
      body(*p);
      u = u->next_unique_;
    } while(u != nullptr);
  }
  
  inline int detail::persona_tls::burst(persona &p, upcxx::progress_level level) {
    constexpr int q_internal = (int)progress_level::internal;
    constexpr int q_user     = (int)progress_level::user;
    
    int exec_n = 0;
    
    UPCXX_ASSERT(!p.not_burstable_[q_user], "An internal action is already trying to burst internal progress.");
    if(!p.not_burstable_[q_user]) {
      p.not_burstable_[q_internal] = true;
      exec_n += p.peer_inbox_.burst(q_internal);
      exec_n += p.self_inbox_.burst(q_internal);
      p.not_burstable_[q_internal] = false;
    }
    
    if(level == progress_level::user) {
      UPCXX_ASSERT(!p.not_burstable_[q_user]);
      p.not_burstable_[q_user] = true;
      exec_n += p.peer_inbox_.burst(q_user);
      exec_n += p.self_inbox_.burst(q_user);
      p.not_burstable_[q_user] = false;
    }
    
    return exec_n;
  }
}
#endif
