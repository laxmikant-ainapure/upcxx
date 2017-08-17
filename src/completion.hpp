#ifndef _96368972_b5ed_4e48_ac4f_8c868279e3dd
#define _96368972_b5ed_4e48_ac4f_8c868279e3dd

#include <upcxx/bind.hpp>
#include <upcxx/future.hpp>

namespace upcxx {

  struct persona {}; // placeholder until it is implemented

  // Completion events
  struct source_cx_event;
  struct remote_cx_event;
  struct operation_cx_event;
  
  //////////////////////////////////////////////////////////////////////
  
  // Signaling actions

  // Future completion
  template<typename Event>
  struct future_cx {};

  // Promise completion
  template<typename Event, typename ...T>
  struct promise_cx {
    promise<T...> &pro_;
  };

  // LPC completion
  template<typename Event, typename Func>
  struct lpc_cx {
    persona &target_;
    Func func_;
    lpc_cx(persona &target, Func func)
      : target_(target), func_(std::move(func)) {}
  };

  // RPC completion. Arguments are bound into fn_.
  template<typename Event, typename Func>
  struct rpc_cx {
    Func fn_;
    rpc_cx(Func fn): fn_{std::move(fn)} {}
  };
  
  //////////////////////////////////////////////////////////////////////
  
  // Completions

  template<typename ...Cxs>
  struct completions {
    std::tuple<Cxs...> cxs;
  };


  template<typename ...ACxs, typename ...BCxs>
  completions<ACxs..., BCxs...> operator|(completions<ACxs...> a,
                                          completions<BCxs...> b) {
    return {std::tuple_cat(std::move(a.cxs), std::move(b.cxs))};
  }

  //////////////////////////////////////////////////////////////////////

  // Interface for obtaining a completion

  // Base template for completions at initiator
  template<typename Event>
  struct here_cx {
    using as_future_t = completions<future_cx<Event>>;

    static constexpr as_future_t as_future() {
      return {};
    }

    template<typename ...T>
    using as_promise_t = completions<promise_cx<Event, T...>>;

    template<typename ...T>
    static as_promise_t<T...> as_promise(promise<T...> &pro) {
      return {std::make_tuple(promise_cx<Event, T...>{pro})};
    }

    template<typename Func>
    using as_lpc_t = completions<lpc_cx<Event, Func>>;

    template<typename Func>
    static as_lpc_t<Func> as_lpc(persona &target, Func func) {
      return {std::make_tuple(lpc_cx<Event, Func>{target, func})};
    }
  };

  // Source and operation completion
  struct source_cx : here_cx<source_cx_event> {};
  struct operation_cx : here_cx<operation_cx_event> {};

  // Remote completion
  struct remote_cx {
    template<typename Func, typename ...Args>
    using rpc_cx_t =
      rpc_cx<remote_cx_event,
             decltype(upcxx::bind(std::declval<Func>(),
                                  std::declval<Args>()...))>;

    template<typename Func, typename ...Args>
    using as_rpc_t = completions<rpc_cx_t<Func, Args...>>;

    template<typename Func, typename ...Args>
    static as_rpc_t<Func, Args...> as_rpc(Func &&func, Args&&... args) {
      return
        {std::make_tuple(rpc_cx_t<Func, Args...>{
                           upcxx::bind(std::forward<Func>(func),
                                       std::forward<Args>(args)...)
                         })};
    }
  };

  //////////////////////////////////////////////////////////////////////

  // Computing a return type

  // A combination of a completion event and the types of values
  // produced
  template<typename Event, typename ...T>
  struct future_return {
    using type = future<T...>;
  };

  namespace detail {

    // Combine void, futures, and tuples of futures
    template<typename ...T>
    struct future_tuple_cat;

    template<typename T>
    struct future_tuple_cat<T, void> {
      using type = T;
    };

    template<typename T>
    struct future_tuple_cat<void, T> {
      using type = T;
    };

    template<>
    struct future_tuple_cat<void, void> {
      using type = void;
    };

    template<typename ...T1, typename ...T2>
    struct future_tuple_cat<future<T1...>, future<T2...>> {
      using type = std::tuple<future<T1...>, future<T2...>>;
    };

    template<typename ...T1, typename ...T2>
    struct future_tuple_cat<future<T1...>, std::tuple<T2...>> {
      using type = std::tuple<future<T1...>, T2...>;
    };

    template<typename ...T1, typename ...T2>
    struct future_tuple_cat<std::tuple<T1...>, future<T2...>> {
      using type = std::tuple<T1..., future<T2...>>;
    };

    template<typename ...T1, typename ...T2>
    struct future_tuple_cat<std::tuple<T1...>, std::tuple<T2...>> {
      using type = std::tuple<T1..., T2...>;
    };

    // Match a completion against a set of future_returns in order to
    // compute the partial return type
    // Unspecialized case: not a future_cx --> void
    template<typename Cx, typename ...Frs>
    struct match {
      using type = void;
    };

    // Specialized: future_cx, but its event does not match the first
    // future_return's --> recurse on rest
    template<typename Event,
             typename Fr1,
             typename ...FrRest>
    struct match<future_cx<Event>, Fr1, FrRest...> {
      using type = typename match<future_cx<Event>, FrRest...>::type;
    };

    // Specialized; future_cx whose event matches that of the first
    // future_return --> pull the types out of the future_return to
    // compute the future type
    template<typename Event,
             typename ...Fr1T,
             typename ...FrRest>
    struct match<future_cx<Event>, future_return<Event, Fr1T...>,
                 FrRest...> {
      using type = future<Fr1T...>;
    };

    // Nested loops over future_returns and completions
    // Outer template variadic over future_returns
    template<typename ...Frs>
    struct future_returns {
      // Inner template variadic over completions
      // unspecialized
      template<typename ...T>
      struct scan;

      // specialized recursive case
      template<typename Cx1, typename ...Cxs>
      struct scan<Cx1, Cxs...> {
        using type =
          typename future_tuple_cat<typename match<Cx1, Frs...>::type,
                                    typename scan<Cxs...>::type>::type;
      };

      // specialized base case for a single completion
      template<typename Cx1>
      struct scan<Cx1> {
        using type = typename match<Cx1, Frs...>::type;
      };

      template<typename>
      struct scan_completions;

      // Pull completions out of the parameter list for the
      // completions template and scan them
      template<typename ...Cxs>
      struct scan_completions<completions<Cxs...>> {
        using type = typename scan<Cxs...>::type;
      };
    };

  } // namespace detail

  // Actual interface for computing a return type; used in combination
  // with future_return
  template<typename Completions, typename ...Frs>
  using cx_return_type =
    typename detail::future_returns<Frs...>::
      template scan_completions<Completions>::type;

  //////////////////////////////////////////////////////////////////////

  // Error checking

  namespace detail {

    // Checking that the type arguments of a requested completion
    // match that of the provided completion event

    // Check if a function is invokable on the given types
    // Placeholder type, where the parameter is used with SFINAE
    template<typename>
    struct placeholder : std::true_type {};

    template<typename Func>
    struct is_invokable {
      // Substitutes on successful invocation
      template<typename ...Args>
      static auto invoke(int) ->
        placeholder<typename std::result_of<Func(Args...)>::type>;
      // Varargs overload for unsuccessful substitution of above
      template<typename ...Args>
      static std::false_type invoke(...);
      // true/false based on whether substitution succeeded
      template<typename ...Args>
      using with = decltype(invoke<Args...>(0));
    };

    // Check an individual completion
    // Unspecialized: everything is OK
    template<typename Event, typename Cx, typename ...T>
    struct check_completion : std::true_type {};

    // Partial specialization for mismatched promise types: not OK
    template<typename Event, typename ...T, typename ...PT>
    struct check_completion<Event, promise_cx<Event, PT...>, T...>
      : std::false_type {};

    // Partial specialization for matched promise types: OK
    template<typename Event, typename ...T>
    struct check_completion<Event, promise_cx<Event, T...>, T...> :
      std::true_type {};

    // Partial specialization to check LPC
    template<typename Event, typename Func, typename ...T>
    struct check_completion<Event, lpc_cx<Event, Func>, T...> {
      static constexpr bool value =
        is_invokable<Func>::template with<T...>::value;
    };

    // Completion event + types of values produced
    template<typename Event, typename ...T>
    struct check_completions_impl {
      // Loop over completions
      // unspecialized base case: true
      template<typename ...Cxs>
      struct check : std::true_type {};

      // specialized recursive case
      template<typename Cx1, typename ...Cxs>
      struct check<Cx1, Cxs...> {
        static constexpr bool value =
          check_completion<Event, Cx1, T...>::value &
          check<Cxs...>::value;
      };

      // Pull completions out of the parameter list for the
      // completions template and scan them
      template<typename>
      struct scan_completions;

      template<typename ...Cxs>
      struct scan_completions<completions<Cxs...>> {
        static constexpr bool value = check<Cxs...>::value;
      };
    };

    // Checking that the event in a requested completion matches a
    // provided completion event

    // Unspecialized: no matching event
    template<typename Cx, typename ...Events>
    struct check_event : std::false_type {};

    // Partial specialization: recursive case for when first event
    // does not match
    template<typename Cx, typename E1, typename ...ERest>
    struct check_event<Cx, E1, ERest...> : check_event<Cx, ERest...> {};

    // Partial specialization: base case for when first event matches
    template<template <typename ...> class CxType,
             typename ...CxArgs, typename Event,
             typename ...ERest>
    struct check_event<CxType<Event, CxArgs...>, Event, ERest...>
      : std::true_type {};

    // Variadic over the set of events provided
    template<typename ...Events>
    struct check_events_impl {
      // Loop over completions
      // unspecialized base case: true
      template<typename ...Cxs>
      struct check : std::true_type {};

      // specialized recursive case
      template<typename Cx1, typename ...Cxs>
      struct check<Cx1, Cxs...> {
        static constexpr bool value =
          check_event<Cx1, Events...>::value &
          check<Cxs...>::value;
      };

      // Pull completions out of the parameter list for the
      // completions template and scan them
      template<typename>
      struct scan_completions;

      template<typename ...Cxs>
      struct scan_completions<completions<Cxs...>> {
        static constexpr bool value = check<Cxs...>::value;
      };
    };

  } // namespace detail

  // Actual interface for checking the types of requested completions
  // against those provided the completion event
  template<typename Completions, typename Event, typename ...T>
  constexpr bool check_completion_types() {
    return detail::check_completions_impl<Event, T...>::
      template scan_completions<Completions>::value;
  }

  // Actual interface for checking the events of requested completions
  // against the events provided
  template<typename Completions, typename ...Events>
  constexpr bool check_completion_events() {
    return detail::check_events_impl<Events...>::
      template scan_completions<Completions>::value;
  }

} // namespace upcxx

#endif
