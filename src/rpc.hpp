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
    backend::template send_am<progress_level_user>(
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
      Pro *pro_;
      
      template<typename ...Args>
      void operator()(Args &&...args) {
        Pro *pro = pro_; // so it gets value-captured, not this-captured.
        
        std::tuple<typename std::decay<Args>::type...> results{
          std::forward<Args>(args)...
        };
        
        backend::template send_am<progress_level_user>(
          initiator_,
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
  }
  
  // rpc: future variant
  template<typename Fn, typename ...Args>
  auto rpc(intrank_t recipient, Fn &&fn, Args &&...args)
    -> detail::future_from_tuple_t<
      detail::future_kind_shref<detail::future_header_ops_result>, // the default future kind
      typename decltype(
        upcxx::apply_tupled_as_future(
          upcxx::bind(std::forward<Fn>(fn), std::forward<Args>(args)...),
          std::tuple<>{}
        )
      )::results_type
    > {
    
    auto fn_bound = upcxx::bind(std::forward<Fn>(fn), std::forward<Args>(args)...);
    
    using Fut = decltype(upcxx::apply_tupled_as_future(fn_bound, std::tuple<>{}));
    using results_type = typename Fut::results_type;
    using Pro = upcxx::tuple_types_into_t<results_type, promise>;
    
    intrank_t initiator = backend::rank_me;
    Pro *pro = new Pro;
    
    backend::template send_am<progress_level_user>(
      recipient,
      upcxx::bind(
        [=](decltype(fn_bound) &fn_bound1) {
          upcxx::apply_tupled_as_future(fn_bound1, std::tuple<>{})
            .then(
              // Wish we could just use a lambda here, but since it has
              // to take variadic Args... we have to call to an outlined
              // class. I'm not sure if even C++14's allowance of `auto`
              // lambda args would be enough.
              detail::rpc_recipient_after<Pro>{initiator, pro}
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
    
    using Fut = decltype(upcxx::apply_tupled_as_future(fn_bound, std::tuple<>{}));
    using results_type = typename Fut::results_type;
    
    intrank_t initiator = backend::rank_me;
    promise<T...> *pro = &prom;
    
    backend::template send_am<progress_level_user>(
      recipient,
      upcxx::bind(
        [=](decltype(fn_bound) &fn_bound1) {
          upcxx::apply_tupled_as_future(fn_bound1, std::tuple<>{})
            .then(
              // Wish we could just use a lambda here, but since it has
              // to take variadic Args... we have to call to an outlined
              // class. I'm not sure if even C++14's allowance of `auto`
              // would be enough.
              detail::rpc_recipient_after<promise<T...>>{initiator, pro}
            );
        },
        std::move(fn_bound)
      )
    );
  }
}
#endif
