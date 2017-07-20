#ifndef _e004d4d0_de3d_4761_a53a_85652b347070
#define _e004d4d0_de3d_4761_a53a_85652b347070

#include <upcxx/future/core.hpp>
#include <upcxx/future/apply.hpp>
#include <upcxx/future/impl_mapped.hpp>

namespace upcxx {
  namespace detail {
    ////////////////////////////////////////////////////////////////////
    // future_body_then: Body type for then's
    
    /* future_body_then_base: base class for future_body_then.
     * separates non-lambda specific code away from lambda specific code
     * to encourage code sharing by the compiler.
     */
    struct future_body_then_base: future_body {
      future_body_then_base(void *storage): future_body{storage} {}
      
      // future_body_then::leave_active calls this after lambda evaluation
      template<typename Kind, typename ...T>
      void leave_active_into_proxy(
          bool pure,
          future_header_dependent *hdr,
          void *storage,
          future1<Kind,T...> proxied
        ) {
        
        // drop one reference for lambda execution if impure,
        // another for active queue.
        if(0 == hdr->refs_drop(pure ? 1 : 2)) {
          // we died
          ::operator delete(storage);
          delete hdr;
        }
        else {
          future_header *proxied_hdr = proxied.impl_.steal_header();
          
          if(Kind::template with_types<T...>::header_ops::is_trivially_ready_result) {
            ::operator delete(storage); // body dead
            // we know proxied_hdr is its own result
            hdr->enter_ready(proxied_hdr);
          }
          else {
            hdr->enter_proxying(
              new(storage) future_body_proxy<T...>(storage),
              proxied_hdr
            );
          }
        }
      }
    };
    
    template<typename FuArg, typename Fn>
    struct future_body_then final: future_body_then_base {
      future_dependency<FuArg> dep_;
      Fn fn_;
      
      future_body_then(
          void *storage, future_header_dependent *hdr,
          FuArg arg, Fn fn
        ):
        future_body_then_base{storage},
        dep_{hdr, std::move(arg)},
        fn_{std::move(fn)} {
      }
      
      // then's can't be destructed early so we inherit the stub
      //void destruct_early();
      
      void leave_active(future_header_dependent *hdr) {
        auto proxied = future_apply<Fn(FuArg)>()(
          this->fn_,
          this->dep_.result_lrefs_getter()()
        );
        
        void *me_mem = this->storage_;
        this->dep_.cleanup_ready();
        this->~future_body_then();
        
        this->leave_active_into_proxy(
          /*pure=*/false, hdr, me_mem, std::move(proxied)
        );
      }
    };
    
    ////////////////////////////////////////////////////////////////////
    // future_body_then_pure: Body type for then_pure's
    
    template<typename FuArg, typename Fn>
    struct future_body_then_pure final: future_body_then_base {
      future_dependency<FuArg> dep_;
      Fn fn_;
      
      future_body_then_pure(
          void *storage, future_header_dependent *hdr,
          FuArg arg, Fn fn
        ):
        future_body_then_base{storage},
        dep_{hdr, std::move(arg)},
        fn_{std::move(fn)} {
      }
      
      // then_pure's can be destructed early so we override stub
      void destruct_early() {
        this->dep_.cleanup_early();
        this->~future_body_then_pure();
      }
      
      void leave_active(future_header_dependent *hdr) {
        auto proxied = future_apply<Fn(FuArg)>()(
          this->fn_,
          this->dep_.result_lrefs_getter()()
        );
        
        void *me_mem = this->storage_;
        this->dep_.cleanup_ready();
        this->~future_body_then_pure();
        
        this->leave_active_into_proxy(
          /*pure=*/true, hdr, me_mem, std::move(proxied)
        );
      }
    };
  } // namespace detail
  
  namespace detail {
    template<typename Arg, typename Fn, typename FnRetKind, typename ...FnRetT>
    struct future_then<
        Arg, Fn, future1<FnRetKind,FnRetT...>,
        /*arg_trivial=*/false
      > {
      
      template<typename Fn1>
      future1<future_kind_shref<future_header_ops_general>, FnRetT...>
      operator()(Arg arg, Fn1 &&fn) {
        future_header_dependent *hdr = new future_header_dependent;
        hdr->ref_n_ += 1; // another for lambda execution
        
        union body_union_t {
          future_body_then<Arg,Fn> then;
          future_body_proxy<FnRetT...> proxy;
        };
        void *storage = ::operator new(sizeof(body_union_t));
        
        future_body_then<Arg,Fn> *body =
          new(storage) future_body_then<Arg,Fn>(
            storage, hdr,
            std::move(arg),
            std::forward<Fn1>(fn)
          );
        hdr->body_ = body;
        
        if(hdr->status_ == future_header::status_active)
          hdr->entered_active();
        
        return future_impl_shref<future_header_ops_general, FnRetT...>(hdr);
      }
    };
    
    template<typename Arg, typename Fn, typename FnRetKind, typename ...FnRetT>
    struct future_then<
        Arg, Fn, future1<FnRetKind,FnRetT...>,
        /*arg_trivial=*/true
      > {
      
      template<typename Fn1>
      future1<future_kind_shref<future_header_ops_general>, FnRetT...>
      operator()(Arg arg, Fn1 &&fn) {
        return future_apply<Fn(Arg)>()(fn, arg.impl_.results_refs());
      }
    };
  } // namespace detail
  
  // a>>b === a.then(b)
  template<typename ArgKind, typename Fn1, typename ...ArgT>
  inline auto operator>>(future1<ArgKind,ArgT...> arg, Fn1 &&fn)
    -> decltype(
      detail::future_then<
        future1<ArgKind,ArgT...>,
        typename std::decay<Fn1>::type
      >()(
        std::move(arg),
        std::forward<Fn1>(fn)
      )
    ) {
    return detail::future_then<
        future1<ArgKind,ArgT...>,
        typename std::decay<Fn1>::type
      >()(
        std::move(arg),
        std::forward<Fn1>(fn)
      );
  }
    
  namespace detail {
    template<typename Arg, typename Fn, typename FnRetKind, typename ...FnRetT>
    struct future_then_pure<
        Arg, Fn, future1<FnRetKind, FnRetT...>,
        /*arg_trivial=*/false,
        /*fnret_trivial=*/false
      > {
      
      template<typename Fn1>
      future1<future_kind_shref<future_header_ops_general>, FnRetT...>
      operator()(Arg arg, Fn1 &&fn) {
        future_header_dependent *hdr = new future_header_dependent;
        
        union body_union_t {
          future_body_then_pure<Arg,Fn> then;
          future_body_proxy<FnRetT...> proxy;
        };
        void *body_mem = ::operator new(sizeof(body_union_t));
        
        hdr->body_ = new(body_mem) future_body_then_pure<Arg,Fn>(
          body_mem, hdr,
          std::move(arg),
          std::forward<Fn1>(fn)
        );
        
        if(hdr->status_ == future_header::status_active)
          hdr->entered_active();
        
        return future_impl_shref<future_header_ops_general, FnRetT...>(hdr);
      }
    };
    
    template<typename Arg, typename Fn, typename FnRetKind, typename ...FnRetT, bool fnret_trivial>
    struct future_then_pure<
        Arg, Fn, future1<FnRetKind, FnRetT...>,
        /*arg_trivial=*/true,
        fnret_trivial
      > {
      template<typename Fn1>
      future1<FnRetKind, FnRetT...> operator()(Arg arg, Fn1 &&fn) {
        return future_apply<Fn(Arg)>()(fn, arg.impl_.results_refs());
      }
    };
    
    template<typename Arg, typename Fn, typename FnRetKind, typename ...FnRetT>
    struct future_then_pure<
        Arg, Fn, future1<FnRetKind, FnRetT...>,
        /*arg_trivial=*/false,
        /*fnret_trivial=*/true
      > {
      template<typename Fn1>
      future1<future_kind_mapped<Arg,Fn>, FnRetT...>
      operator()(Arg arg, Fn1 &&fn) {
        return future_impl_mapped<Arg,Fn,FnRetT...>{std::move(arg), std::forward<Fn1>(fn)};
      }
    };
  } // namespace detail
}
#endif
