#ifndef _cfdca0c3_057d_4ee9_9c82_b68e4bc96a0f
#define _cfdca0c3_057d_4ee9_9c82_b68e4bc96a0f

#include <upcxx/backend.hpp>
#include <upcxx/bind.hpp>
#include <upcxx/completion.hpp>
#include <upcxx/future.hpp>

namespace upcxx {
  //////////////////////////////////////////////////////////////////////
  // rpc_ff

  namespace detail {
    struct rpc_ff_event_values {
      template<typename Event>
      using tuple_t = std::tuple<>;
    };
    
    template<typename Call, typename Cxs,
             typename ValidType = typename std::result_of<Call>::type>
    struct rpc_ff_return;

    template<typename Fn, typename ...Arg, typename Cxs, typename ValidType>
    struct rpc_ff_return<Fn(Arg...), Cxs, ValidType> {
      using type = typename detail::completions_returner<
          /*EventPredicate=*/detail::event_is_here,
          /*EventValues=*/detail::rpc_ff_event_values,
          Cxs
        >::return_t;
    };
  }
  
  // defaulted completions
  template<typename Fn, typename ...Args>
  auto rpc_ff(intrank_t recipient, Fn &&fn, Args &&...args)
    // computes our return type, but SFINAE's out if fn(args...) is ill-formed
    -> typename detail::rpc_ff_return<Fn(Args...), completions<>>::type {
    
    backend::template send_am_master<progress_level::user>(
      recipient,
      upcxx::bind(std::forward<Fn>(fn), std::forward<Args>(args)...)
    );
  }

  // explicit completions
  template<typename Cxs, typename Fn, typename ...Args>
  auto rpc_ff(intrank_t recipient, Cxs cxs, Fn &&fn, Args &&...args)
    // computes our return type, but SFINAE's out if fn(args...) is ill-formed
    -> typename detail::rpc_ff_return<Fn(Args...), Cxs>::type {

    auto state = detail::completions_state<
        /*EventPredicate=*/detail::event_is_here,
        /*EventValues=*/detail::rpc_ff_event_values,
        Cxs
      >{std::move(cxs)};
    
    auto returner = detail::completions_returner<
        /*EventPredicate=*/detail::event_is_here,
        /*EventValues=*/detail::rpc_ff_event_values,
        Cxs
      >{state};
    
    backend::template send_am_master<progress_level::user>(
      recipient,
      upcxx::bind(std::forward<Fn>(fn), std::forward<Args>(args)...)
    );
    
    // send_am_master doesn't support async source-completion, so we know
    // its trivially satisfied.
    state.template operator()<source_cx_event>();
    
    return returner();
  }

  //////////////////////////////////////////////////////////////////////
  // rpc
  
  namespace detail {
    template<typename CxsState>
    struct rpc_recipient_after {
      intrank_t initiator_;
      persona *initiator_persona_;
      CxsState *state_;

      struct operation_satisfier {
        CxsState *state;
        template<typename ...T>
        void operator()(T &&...vals) {
          state->template operator()<operation_cx_event>(std::forward<T>(vals)...);
          delete state;
        }
      };
      
      template<typename ...Args>
      void operator()(Args &&...args) {
        backend::template send_am_persona<progress_level::user>(
          initiator_,
          initiator_persona_,
          upcxx::bind(operation_satisfier{state_}, std::forward<Args>(args)...)
        );
      }
    };

    template<typename Fn, typename ...Args>
    struct rpc_event_values {
      template<typename Event>
      using tuple_t = typename std::conditional<
          std::is_same<Event, operation_cx_event>::value,
          /*Event == operation_cx_event:*/typename decltype(
            upcxx::apply_tupled_as_future(
              upcxx::bind(std::declval<Fn&&>(), std::declval<Args&&>()...),
              std::tuple<>{}
            )
          )::results_type,
          /*Event != operation_cx_event:*/std::tuple<>
        >::type;
    };

    // Computes return type for rpc. ValidType is unused, but by requiring
    // it we can predicate this instantion on the validity of ValidType.
    template<typename Call, typename Cxs,
             typename ValidType = typename std::result_of<Call>::type>
    struct rpc_return;

    template<typename Fn, typename ...Args, typename Cxs, typename ValidType>
    struct rpc_return<Fn(Args...), Cxs, ValidType> {
      using type = typename detail::completions_returner<
          /*EventPredicate=*/detail::event_is_here,
          /*EventValues=*/detail::rpc_event_values<Fn,Args...>,
          Cxs
        >::return_t;
    };
  }
  
  template<typename Cxs, typename Fn, typename ...Args>
  auto rpc(intrank_t recipient, Cxs cxs, Fn &&fn, Args &&...args)
    // computes our return type, but SFINAE's out if fn(args...) is ill-formed
    -> typename detail::rpc_return<Fn(Args...), Cxs>::type {
    
    using cxs_state_t = detail::completions_state<
        /*EventPredicate=*/detail::event_is_here,
        /*EventValues=*/detail::rpc_event_values<Fn,Args...>,
        Cxs
      >;
    
    cxs_state_t *state = new cxs_state_t{std::move(cxs)};
    
    auto returner = detail::completions_returner<
        /*EventPredicate=*/detail::event_is_here,
        /*EventValues=*/detail::rpc_event_values<Fn,Args...>,
        Cxs
      >{*state};

    intrank_t initiator = backend::rank_me;
    persona *initiator_persona = &upcxx::current_persona();
    auto fn_bound = upcxx::bind(std::forward<Fn>(fn), std::forward<Args>(args)...);
    
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
              detail::rpc_recipient_after<cxs_state_t>{
                initiator, initiator_persona, state
              }
            );
        },
        std::move(fn_bound)
      )
    );
    
    // send_am_master doesn't support async source-completion, so we know
    // its trivially satisfied.
    state->template operator()<source_cx_event>();
    
    return returner();
  }

  // rpc: default completions variant
  template<typename Fn, typename ...Args>
  auto rpc(intrank_t recipient, Fn &&fn, Args &&...args)
    -> decltype(
      upcxx::template rpc<completions<future_cx<operation_cx_event>>, Fn, Args...>(
        recipient, operation_cx::as_future(),
        static_cast<Fn&&>(fn), static_cast<Args&&>(args)...
      )
    ) {
    return upcxx::template rpc<completions<future_cx<operation_cx_event>>, Fn, Args...>(
      recipient, operation_cx::as_future(),
      static_cast<Fn&&>(fn), static_cast<Args&&>(args)...
    );
  }
}
#endif
