#ifndef _cfdca0c3_057d_4ee9_9c82_b68e4bc96a0f
#define _cfdca0c3_057d_4ee9_9c82_b68e4bc96a0f

#include <upcxx/backend.hpp>
#include <upcxx/bind.hpp>
#include <upcxx/future.hpp>

namespace upcxx {
  //////////////////////////////////////////////////////////////////////
  // rpc_ff
  
  template<typename Fn, typename ...Args>
  void rpc_ff(intrank_t recipient, Fn &&fn, Args &&...args) {
    backend::template send_am_master<progress_level::user>(
      recipient,
      upcxx::bind(std::forward<Fn>(fn), std::forward<Args>(args)...)
    );
  }
  
  //////////////////////////////////////////////////////////////////////
  // rpc
  
  namespace detail {
    template<typename Pro>
    struct rpc_recipient_after {
      intrank_t initiator_;
      persona *initiator_persona_;
      Pro *pro_;
      
      template<typename ...Args>
      void operator()(Args &&...args) {
        Pro *pro = pro_; // so it gets value-captured, not this-captured.
        
        std::tuple<typename std::decay<Args>::type...> results{
          std::forward<Args>(args)...
        };
        
        backend::template send_am_persona<progress_level::user>(
          initiator_,
          initiator_persona_,
          upcxx::bind(
            [=](decltype(results) const &results1) {
              pro->fulfill_result(std::move(results1));
              delete pro;
            },
            std::move(results)
          )
        );
      }
    };
    
    // Computes return type for rpc. ValidType is unused, but by requiring
    // it we can predicate this instantion on the validity of ValidType.
    template<typename ValidType, typename Fn, typename ...Args>
    struct rpc_return {
      using type = typename detail::future_from_tuple_t<
        detail::future_kind_shref<detail::future_header_ops_result>, // the default future kind
        typename decltype(
          upcxx::apply_tupled_as_future(
            upcxx::bind(std::declval<Fn&&>(), std::declval<Args&&>()...),
            std::tuple<>{}
          )
        )::results_type
      >;
    };
  }
  
  // rpc: future variant
  template<typename Fn, typename ...Args>
  auto rpc(intrank_t recipient, Fn &&fn, Args &&...args)
    // computes our return type, but SFINAE's out if fn(args...) is ill-formed
    -> typename detail::rpc_return<decltype(fn(args...)), Fn, Args...>::type {
    
    auto fn_bound = upcxx::bind(std::forward<Fn>(fn), std::forward<Args>(args)...);
    
    using Fut = decltype(upcxx::apply_tupled_as_future(fn_bound, std::tuple<>{}));
    using results_type = typename Fut::results_type;
    using Pro = upcxx::tuple_types_into_t<results_type, promise>;
    
    intrank_t initiator = backend::rank_me;
    persona *initiator_persona = &upcxx::current_persona();
    Pro *pro = new Pro;
    
    backend::template send_am_master<progress_level::user>(
      recipient,
      upcxx::bind(
        [=](decltype(fn_bound) &fn_bound1) {
          upcxx::apply_tupled_as_future(fn_bound1, std::tuple<>{})
            .then(
              // Wish we could just use a lambda here, but since it has
              // to take variadic Args... we have to call to an outlined
              // class. I'm not sure if even C++14's allowance of `auto`
              // lambda args would be enough.
              detail::rpc_recipient_after<Pro>{initiator, initiator_persona, pro}
            );
        },
        std::move(fn_bound)
      )
    );
    
    return pro->get_future();
  }
  
  // rpc: promise variant
  template<typename Fn, typename ...T, typename ...Args>
  void rpc(intrank_t recipient, promise<T...> &prom, Fn &&fn, Args &&...args) {
    auto fn_bound = upcxx::bind(std::forward<Fn>(fn), std::forward<Args>(args)...);
    
    //using Fut = decltype(upcxx::apply_tupled_as_future(fn_bound, std::tuple<>{}));
    //using results_type = typename Fut::results_type;
    
    intrank_t initiator = backend::rank_me;
    persona *initiator_persona = &upcxx::current_persona();
    promise<T...> *pro = &prom;
    
    backend::template send_am_master<progress_level::user>(
      recipient,
      upcxx::bind(
        [=](decltype(fn_bound) &fn_bound1) {
          upcxx::apply_tupled_as_future(fn_bound1, std::tuple<>{})
            .then(
              // Wish we could just use a lambda here, but since it has
              // to take variadic Args... we have to call to an outlined
              // class. I'm not sure if even C++14's allowance of `auto`
              // would be enough.
              detail::rpc_recipient_after<promise<T...>>{initiator, initiator_persona, pro}
            );
        },
        std::move(fn_bound)
      )
    );
  }
}
#endif
