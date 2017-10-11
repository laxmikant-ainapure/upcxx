#ifndef _4281eee2_6d52_49d0_8126_75b21f8cb178
#define _4281eee2_6d52_49d0_8126_75b21f8cb178

#include <upcxx/diagnostic.hpp>
#include <upcxx/utility.hpp>

namespace upcxx {
  //////////////////////////////////////////////////////////////////////
  // Forwards of internal types.
  
  namespace detail {
    struct future_header;
    template<typename ...T>
    struct future_header_result;
    struct future_header_dependent;
    
    template<typename FuArg>
    struct future_dependency;
    
    struct future_body;
    struct future_body_proxy_;
    template<typename ...T>
    struct future_body_proxy;
    template<typename FuArg>
    struct future_body_pure;
    template<typename FuArg, typename Fn>
    struct future_body_then;
    
    // classes of all-static functions for working with headers
    struct future_header_ops_general;
    struct future_header_ops_result;
    struct future_header_ops_result_ready;
    
    // future implementations
    template<typename HeaderOps, typename ...T>
    struct future_impl_shref;
    template<typename ...T>
    struct future_impl_result;
    template<typename ArgTuple, typename ...T>
    struct future_impl_when_all;
    template<typename FuArg, typename Fn, typename ...T>
    struct future_impl_mapped;
    
    // future1 implementation mappers
    template<typename HeaderOps>
    struct future_kind_shref {
      template<typename ...T>
      using with_types = future_impl_shref<HeaderOps,T...>;
    };
    
    struct future_kind_result {
      template<typename ...T>
      using with_types = future_impl_result<T...>;
    };
    
    template<typename ...FuArg>
    struct future_kind_when_all {
      template<typename ...T>
      using with_types = future_impl_when_all<std::tuple<FuArg...>, T...>;
    };
    
    template<typename FuArg, typename Fn>
    struct future_kind_mapped {
      template<typename ...T>
      using with_types = future_impl_mapped<FuArg,Fn,T...>;
    };
  }
  
  //////////////////////////////////////////////////////////////////////
  // future1: The type given to users.
  // implemented in: upcxx/future/future1.hpp
  
  template<typename Kind, typename ...T>
  struct future1;
  
  //////////////////////////////////////////////////////////////////////
  // future: An alias for future1 using a shared reference implementation.
  
  template<typename ...T>
  using future = future1<
    detail::future_kind_shref<detail::future_header_ops_general>,
    T...
  >;
  
  //////////////////////////////////////////////////////////////////////
  // future_is_trivially_ready: Trait for detecting trivially ready
  // futures. Specializations provided in each future implementation.
  
  template<typename FutureOrKind>
  struct future_is_trivially_ready {
    static constexpr bool value = false;
  };
  
  //////////////////////////////////////////////////////////////////////
  // Future/continuation function-application support
  // implemented in: upcxx/future/apply.hpp
  
  namespace detail {
    // Apply function to tupled arguments lifting return to future.
    // Defined in: future/apply.hpp
    template<typename Fn, typename ArgTup>
    struct apply_tupled_as_future/*{
      typedef future1<Kind,U...> return_type;
      return_type operator()(Fn &&fn, std::tuple<T...> &&arg);
    }*/;
    
    // Apply function to results of future with return lifted to future.
    template<typename App> // App = Fn(future1<Kind,T...>)
    struct apply_futured_as_future/*{
      typedef future1<Kind,U...> return_type;
      return_type operator()(Fn &&fn, future1<Kind,T...> &&arg);
    }*/;
    
    template<typename App>
    using apply_futured_as_future_return_t = typename apply_futured_as_future<App>::return_type;
  }

  //////////////////////////////////////////////////////////////////////
  // detail::future_from_tuple: Generate future1 type from a Kind and
  // result types in a tuple.
  
  namespace detail {
    template<typename Kind, typename Tup>
    struct future_from_tuple;
    template<typename Kind, typename ...T>
    struct future_from_tuple<Kind, std::tuple<T...>> {
      using type = future1<Kind, T...>;
    };
    
    template<typename Kind, typename Tup>
    using future_from_tuple_t = typename future_from_tuple<Kind,Tup>::type;
  }
  
  //////////////////////////////////////////////////////////////////////
  // detail::future_then()(a,b): implementats `a.then(b)`
  // detail::future_then_pure()(a,b): implementats `a.then_pure(b)`
  // implemented in: upcxx/future/then.hpp
  
  namespace detail {
    template<
      typename Arg, typename Fn,
      typename FnRet = apply_futured_as_future_return_t<Fn(Arg)>,
      bool arg_trivial = future_is_trivially_ready<Arg>::value>
    struct future_then;
    
    template<
      typename Arg, typename Fn,
      typename FnRet = apply_futured_as_future_return_t<Fn(Arg)>,
      bool arg_trivial = future_is_trivially_ready<Arg>::value,
      bool fnret_trivial = future_is_trivially_ready<FnRet>::value>
    struct future_then_pure;
  }
  
  //////////////////////////////////////////////////////////////////////
  // future headers...
  
  namespace detail {
    struct future_header {
      // Our refcount. A negative value indicates a static lifetime.
      int ref_n_;
      
      // Status codes:
      static constexpr int 
        // Any greater-than-zero status value means we are waiting for
        // that many other futures until we hit "status_active". When
        // one future notifies a dependent future of its completion,
        // it simply decrements the dependent's status.

        // This future's dependencies have all finished, but this one
        // needs its body's `leave_active()` called.
        status_active = 0,
        
        // This future finished. Its `result_` member points to a
        // future_header_result<T...> containing the result value.
        status_ready = -1,
        
        // This future is proxying for another one which is not
        // finished. Has body of type `future_body_proxy<T...>`.
        status_proxying = -2,
        
        // This future is proxying for another future which is ready.
        // We need to be marked as ready too.
        status_proxying_active = -3;
      
      int status_;
      
      struct dependency_link {
        // The dependency future: one being waited on.
        future_header *dep;
        
        // The successor future: one waiting for dependency.
        future_header_dependent *suc; // nullptr if not in a successor list
        
        // List pointer for dependency's successor list.
        dependency_link *sucs_next;
        
        void unlink();
      };
      
      // List of successor future headers waiting on us.
      dependency_link *sucs_head_;
      
      union {
        // Used when: status_ == status_ready.
        // Points to the future_header_result<T...> holding our results,
        // which might actually be "this".
        future_header *result_;
        
        // Used when: status_ != status_ready && this is instance of future_header_dependent.
        // Holds callback to know what to do when we become active.
        future_body *body_;
      };
      
      // Tell this header to enter the ready state, will adopt `result`
      // as its result header (assumes its refcount is already incremented).
      void enter_ready(future_header *result);
      
      // The "nil" future, not to be used. Only exists so that future_impl_shref's don't
      // have to test for nullptr, instead they'll just see its negative ref_n_.
      static future_header the_nil;
      
      // Modify the refcount, but do not take action.
      inline void refs_add(int n) {
        int ref_n = this->ref_n_;
        int trash;
        (ref_n >= 0 ? this->ref_n_ : trash) = ref_n + n;
      }
      inline int refs_drop(int n) { // returns new refcount
        int ref_n = this->ref_n_;
        bool write_back = ref_n >= 0;
        ref_n -= (ref_n >= 0 ? n : 0);
        int trash;
        (write_back ? this->ref_n_ : trash) = ref_n;
        return ref_n;
      }
      
      // Drop reference to "a" in favor of "a->result_" (returned).
      static future_header* drop_for_result(future_header *a);
      
      // Drop reference to "a" in favor of its proxied header (returned).
      // "a->status_" must be "status_proxying" or "status_proxying_active".
      static future_header* drop_for_proxied(future_header *a);
    };
    
    
    ////////////////////////////////////////////////////////////////////
    // future_header_dependent: dependent headers are those that...
    // - Wait for other futures to finish and then fire some specific action.
    // - Use their bodies to store their state while in wait.
    // - Don't store their own results, their "result_" points to the
    //   future_header_result<T...> holding the result.
    
    struct future_header_dependent final: future_header {
      // For our potential members ship in the singly-linked "active queue".
      future_header_dependent *active_next_;
      
      future_header_dependent() {
        this->ref_n_ = 1;
        this->status_ = future_header::status_active;
        this->sucs_head_ = nullptr;
      }
      
      // Notify engine that this future has just entered an active state.
      // The "status_" must be "status_active" or "status_proxying_active".
      void entered_active();
      
      // Put this future into the "status_proxying" state using a constructed
      // future_body_proxy<T...> instance and the future to be proxied's header.
      void enter_proxying(future_body_proxy_ *body, future_header *proxied);
      
      // Override refcount arithmetic with more efficient form since we
      // know future_header_dependents are never statically allocated.
      inline void refs_add(int n) {
        this->ref_n_ += n;
      }
      inline int refs_drop(int n) {
        return (this->ref_n_ -= n);
      }
    };
    
    
    ////////////////////////////////////////////////////////////////////
    // future_header_ops_general: Header operations given no special
    // knowledge.
    
    struct future_header_ops_general {
      static constexpr bool is_trivially_ready_result = false;
      
      template<typename ...T>
      static void decref_header(future_header *hdr);
      template<typename ...T>
      static void delete_header(future_header *hdr);
    };
    
    
    ////////////////////////////////////////////////////////////////////
    // future_header_ops_result: Header operations given this header is
    // an instance of future_header_result<T...>.
    
    struct future_header_ops_result: future_header_ops_general {
      static constexpr bool is_trivially_ready_result = false;
      
      template<typename ...T>
      static void decref_header(future_header *hdr);
      template<typename ...T>
      static void delete_header(future_header *hdr);
    };
    
    
    ////////////////////////////////////////////////////////////////////
    // future_header_ops_result: Header operations given this header is
    // an instance of future_header_result<T...> and is ready.
    
    struct future_header_ops_result_ready: future_header_ops_result {
      static constexpr bool is_trivially_ready_result = true;
      
      template<typename ...T>
      static void decref_header(future_header *hdr);
      template<typename ...T>
      static void delete_header(future_header *hdr);
    };
    
    
    ////////////////////////////////////////////////////////////////////
    // future_body: Companion objects to headers that hold the
    // polymorphic features of the future.
    
    // Base type for all future bodies.
    struct future_body {
      // The memory block holding this body. Managed by ::operator new/delete().
      void *storage_;
      
      future_body(void *storage): storage_{storage} {}
      
      // Tell this body to destruct itself (but not delete storage) given
      // that it hasn't had "leave_active" called. Default implementation
      // should never be called.
      virtual void destruct_early();
      
      // Tell this body that its time to leave the "active" state by running
      // its specific action.
      virtual void leave_active(future_header_dependent *owner_hdr) = 0;
    };
    
    // Base type for bodies that proxy other futures.
    struct future_body_proxy_: future_body {
      // Our link in the proxied-for future's successor list.
      future_header::dependency_link link_;
      
      future_body_proxy_(void *storage): future_body{storage} {}
      
      void leave_active(future_header_dependent *owner_hdr);
    };
    
    template<typename ...T>
    struct future_body_proxy final: future_body_proxy_ {
      future_body_proxy(void *storage): future_body_proxy_{storage} {}
      
      void destruct_early() {
        this->link_.unlink();
        future_header_ops_general::template decref_header<T...>(this->link_.dep);
        this->~future_body_proxy();
      }
    };
    
    
    ////////////////////////////////////////////////////////////////////
    // future_header_result<T...>: Header containing the result values
    
    template<typename ...T>
    struct future_header_result final: future_header {
      static constexpr int status_results_yes = status_active + 1;
      static constexpr int status_results_no = status_active + 2;
      
      union { std::tuple<T...> results_; };
      
      future_header_result():
        future_header{
          /*ref_n_*/1,
          /*status_*/status_results_no,
          /*sucs_head_*/nullptr,
          {/*result_*/this}
        } {
      }
      
      template<typename ...U>
      future_header_result(bool not_ready, std::tuple<U...> values):
        future_header{
          /*ref_n_*/1,
          /*status_*/not_ready ? status_results_yes : status_ready,
          /*sucs_head_*/nullptr,
          {/*result_*/this}
        },
        results_{std::move(values)} {
      }
      
    private:
      // force outsiders to use delete_me() and delete_me_ready()
      ~future_header_result() {}
    
    public:
      // static_cast `hdr` to future_header_result<T...> and retrieve
      // results tuple.
      static std::tuple<T...>& results_of(future_header *hdr) {
        return static_cast<future_header_result<T...>*>(hdr)->results_;
      }
      
      void readify() {
        UPCXX_ASSERT(this->status_ == status_results_yes);
        this->enter_ready(this);
      }
      
      template<typename ...U>
      void construct_results(U &&...values) {
        UPCXX_ASSERT(this->status_ == status_results_no);
        new(&this->results_) std::tuple<T...>{std::forward<U>(values)...};
        this->status_ = status_results_yes;
      }
      
      template<typename ...U>
      void construct_results(std::tuple<U...> &&values) {
        UPCXX_ASSERT(this->status_ == status_results_no);
        new(&this->results_) std::tuple<T...>{std::move(values)};
        this->status_ = status_results_yes;
      }
      
      void delete_me_ready() {
        typedef std::tuple<T...> results_t;
        results_.~results_t();
        delete this;
      }
      
      void delete_me() {
        if(this->status_ == status_results_yes ||
           this->status_ == status_ready) {
          typedef std::tuple<T...> results_t;
          results_.~results_t();
        }
        delete this;
      }
    };
    
    template<>
    struct future_header_result<> final: future_header {
      static future_header the_always;
      
      enum {
        status_not_ready = status_active + 1
      };
      
      future_header_result():
        future_header{
          /*ref_n_*/1,
          /*status_*/status_not_ready,
          /*sucs_head_*/nullptr,
          {/*result_*/this}
        } {
      }
      
      future_header_result(bool not_ready, std::tuple<>):
        future_header{
          /*ref_n_*/1,
          /*status_*/not_ready ? status_not_ready : status_ready,
          /*sucs_head_*/nullptr,
          {/*result_*/this}
        } {
      }
      
      static std::tuple<> results_of(future_header *hdr) {
        return std::tuple<>{};
      }
      
      void readify() {
        UPCXX_ASSERT(this->status_ == status_not_ready);
        this->enter_ready(this);
      }
      
      void construct_results() {}
      void construct_results(std::tuple<>) {}
      
      void delete_me_ready() { delete this; }
      void delete_me() { delete this; }
    };
    
    
    ////////////////////////////////////////////////////////////////////
    // future_header_ops_general implementation
    
    template<typename ...T>
    void future_header_ops_general::delete_header(future_header *hdr) {
      // Common case is deleting a ready future.
      if(hdr->status_ == future_header::status_ready) {
        future_header *result = hdr->result_;
        
        if(result == hdr)
          static_cast<future_header_result<T...>*>(hdr)->delete_me_ready();
        else {
          // Drop ref to our result.
          if(0 == result->refs_drop(1))
            static_cast<future_header_result<T...>*>(result)->delete_me_ready();
          // Since we're ready we have no body, just delete the header.
          delete static_cast<future_header_dependent*>(hdr);
        }
      }
      // Future dying prematurely.
      else {
        future_header *result = hdr->result_;
        
        if(result == hdr) {
          // Not ready but is its own result, must be a promise.
          static_cast<future_header_result<T...>*>(hdr)->delete_me();
        }
        else {
          // Only case that requires polymorphic destruction.
          future_header_dependent *hdr1 = static_cast<future_header_dependent*>(hdr);
          future_body *body = hdr1->body_;
          void *storage = body->storage_;
          body->destruct_early();
          ::operator delete(storage);
          delete hdr1;
        }
      }
    }

    template<typename ...T>
    inline void future_header_ops_general::decref_header(future_header *hdr) {
      if(0 == hdr->refs_drop(1))
        delete_header<T...>(hdr);
    }
    
    
    ////////////////////////////////////////////////////////////////////
    // future_header_ops_result implementation
    
    template<typename ...T>
    void future_header_ops_result::delete_header(future_header *hdr) {
      static_cast<future_header_result<T...>*>(hdr)->delete_me();
    } 
    
    template<typename ...T>
    void future_header_ops_result::decref_header(future_header *hdr) {
      if(0 == hdr->refs_drop(1))
        static_cast<future_header_result<T...>*>(hdr)->delete_me();
    }
    
    
    ////////////////////////////////////////////////////////////////////
    // future_header_ops_result_ready implementation
    
    template<typename ...T>
    void future_header_ops_result_ready::delete_header(future_header *hdr) {
      static_cast<future_header_result<T...>*>(hdr)->delete_me_ready();
    } 
    
    template<typename ...T>
    void future_header_ops_result_ready::decref_header(future_header *hdr) {
      if(0 == hdr->refs_drop(1))
        static_cast<future_header_result<T...>*>(hdr)->delete_me_ready();
    }
    
    
    ////////////////////////////////////////////////////////////////////
    
    inline future_header* future_header::drop_for_result(future_header *a) {
      future_header *b = a->result_;
      
      int a_refs = a->ref_n_;
      int a_unit = a_refs < 0 || a == b ? 0 : 1;
      
      int b_refs = b->ref_n_;
      int b_unit = b_refs < 0 || a == b ? 0 : 1;
      
      a_refs -= a_unit;
      b_refs += b_unit;
      
      if(0 == a_refs)
        // deleting a so b loses that reference
        b_refs -= b_unit;
      
      // write back a->ref_n_
      {int trash; (a_unit == 1 ? a->ref_n_ : trash) = a_refs;}
      // write back b->ref_n_
      {int trash; (b_unit == 1 ? b->ref_n_ : trash) = b_refs;}
      
      if(0 == a_refs) {
        // must be a dependent since if it were a result it couldn't have zero refs
        delete static_cast<future_header_dependent*>(a);
      }
      
      return b;
    }
    
    inline future_header* future_header::drop_for_proxied(future_header *a) {
      future_body_proxy_ *a_body = static_cast<future_body_proxy_*>(a->body_);
      future_header *b = a_body->link_.dep;
      
      int a_refs = a->ref_n_;
      int b_refs = b->ref_n_;
      int b_unit = b_refs < 0 ? 0 : 1;
      
      a_refs -= 1;
      b_refs += b_unit;
      
      if(0 == a_refs)
        // deleting a so b loses that reference
        b_refs -= b_unit;
      
      // write back a->ref_n_
      a->ref_n_ = a_refs;
      // write back b->ref_n_
      {int trash; (b_unit == 1 ? b->ref_n_ : trash) = b_refs;}
      
      if(0 == a_refs) {
        if(a->status_ == status_proxying)
          a_body->link_.unlink();
        // proxy bodies are trivially destructible
        ::operator delete(a_body->storage_);
        // proxying headers are dependents
        delete static_cast<future_header_dependent*>(a);
      }
      
      return b;
    }
  } // namespace detail
}
#endif
