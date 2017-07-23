#ifndef _cfdca0c3_057d_4ee9_9c82_b68e4bc96a0f
#define _cfdca0c3_057d_4ee9_9c82_b68e4bc96a0f

#include <upcxx/backend.hpp>
#include <upcxx/bind.hpp>
#include <upcxx/future.hpp>

namespace upcxx {
  template<typename Fn, typename ...Args>
  void rpc_ff(intrank_t recipient, Fn &&fn, Args &&...args) {
    backend::template send_am<progress_level_user>(
      recipient,
      upcxx::bind(std::forward<Fn>(fn), std::forward<Args>(args)...)
    );
  }
  
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
            results
          )
        );
      }
    };
  }
  
  template<typename Fn, typename ...Args>
  auto rpc(intrank_t recipient, Fn &&fn, Args &&...args)
    -> detail::future_from_tuple_t<
      detail::future_kind_shref<detail::future_header_ops_result>,
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
            .then(detail::rpc_recipient_after<Pro>{initiator, pro});
        },
        std::move(fn_bound)
      )
    );
    
    return pro->get_future();
  }
}
#endif
