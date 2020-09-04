#ifndef _96368972_b5ed_4e48_ac4f_8c868279e3dd
#define _96368972_b5ed_4e48_ac4f_8c868279e3dd

#include <upcxx/backend.hpp>
#include <upcxx/bind.hpp>
#include <upcxx/future.hpp>
#include <upcxx/lpc_dormant.hpp>
#include <upcxx/persona.hpp>
#include <upcxx/utility.hpp>

#include <tuple>

namespace upcxx {
  //////////////////////////////////////////////////////////////////////////////
  /* Event names for common completion events as used by rput/rget etc. This
  set is extensible from anywhere in the source. These are left as incomplete
  types since only their names matter as template parameters (usually spelled
  "Event" in this file). Spelling is `[what]_cx_event`.
  */
  
  struct source_cx_event;
  struct remote_cx_event;
  struct operation_cx_event;

  namespace detail {
    // Useful type predicates for selecting events (as needed by
    // completions_state's EventPredicate argument).
    template<typename Event>
    struct event_is_here: std::false_type {};
    template<>
    struct event_is_here<source_cx_event>: std::true_type {};
    template<>
    struct event_is_here<operation_cx_event>: std::true_type {};

    template<typename Event>
    struct event_is_remote: std::false_type {};
    template<>
    struct event_is_remote<remote_cx_event>: std::true_type {};
  }
  
  //////////////////////////////////////////////////////////////////////////////
  /* Completion action descriptors holding the information provided by the
  user. Spelling is `[what]_cx`. The type encodes the event this action
  corresponds to. The runtime state should hold copies of whatever runtime
  state the user supplied with no extra processing. The corresponding event can
  be queried via `::event_t`. Since the `rpc_cx` action is shipped remotely to
  fulfill `as_rpc`, to make the other templates manageable, all actions must
  "pretend" to also support serialization by supplying a `::deserialized_cx`
  type to be used as its `deserialized_type`.
  */

  // Future completion to be fulfilled during given progress level
  template<typename Event, progress_level level = progress_level::user>
  struct future_cx {
    using event_t = Event;
    using deserialized_cx = future_cx<Event,level>;
    // stateless
  };

  // Promise completion
  template<typename Event, typename ...T>
  struct promise_cx {
    using event_t = Event;
    using deserialized_cx = promise_cx<Event,T...>;
    detail::promise_shref<T...> pro_;
  };

  // Synchronous completion via best-effort buffering
  template<typename Event>
  struct buffered_cx {
    using event_t = Event;
    using deserialized_cx = buffered_cx<Event>;
    // stateless
  };

  // Synchronous completion via blocking on network/peers
  template<typename Event>
  struct blocking_cx {
    using event_t = Event;
    using deserialized_cx = blocking_cx<Event>;
    // stateless
  };

  // LPC completion
  template<typename Event, typename Fn>
  struct lpc_cx {
    using event_t = Event;
    using deserialized_cx = lpc_cx<Event,Fn>;
    
    persona *target_;
    Fn fn_;

    template<typename Fn1>
    lpc_cx(persona &target, Fn1 &&fn):
      target_(&target),
      fn_(std::forward<Fn1>(fn)) {
    }
  };
  
  // RPC completion. Arguments are already bound into fn_ (via upcxx::bind).
  template<typename Event, typename Fn>
  struct rpc_cx {
    using event_t = Event;
    using deserialized_cx = rpc_cx<Event, typename serialization_traits<Fn>::deserialized_type>;
    
    Fn fn_;
    rpc_cx(Fn fn): fn_(std::move(fn)) {}
  };
  
  //////////////////////////////////////////////////////////////////////////////
  /* completions<...>: A list of completion actions. We use lisp-like lists where
  the head is the first element and the tail is the list of everything after.
  */
  
  template<typename ...Cxs>
  struct completions;
  template<>
  struct completions<> {};
  template<typename H, typename ...T>
  struct completions<H,T...>: completions<T...> {
    H head;

    H&& head_moved() {
      return static_cast<H&&>(head);
    }
    completions<T...>&& tail_moved() {
      return static_cast<completions<T...>&&>(*this);
    }
    
    constexpr completions(H head, T ...tail):
      completions<T...>(std::move(tail)...),
      head(std::move(head)) {
    }
    constexpr completions(H head, completions<T...> tail):
      completions<T...>(std::move(tail)),
      head(std::move(head)) {
    }
  };

  //////////////////////////////////////////////////////////////////////////////
  // operator "|": Concatenates two completions lists.
  
  template<typename ...B>
  constexpr completions<B...> operator|(
      completions<> a, completions<B...> b
    ) {
    return b;
  }
  template<typename Ah, typename ...At, typename ...B>
  constexpr completions<Ah,At...,B...> operator|(
      completions<Ah,At...> a,
      completions<B...> b
    ) {
    return completions<Ah,At...,B...>{
      a.head_moved(),
      a.tail_moved() | std::move(b)
    };
  }

  //////////////////////////////////////////////////////////////////////////////
  // detail::completions_has_event: detects if there exists an action associated
  // with the given event in the completions list.

  namespace detail {
    template<typename Cxs, typename Event>
    struct completions_has_event;
    
    template<typename Event>
    struct completions_has_event<completions<>, Event> {
      static constexpr bool value = false;
    };
    template<typename CxH, typename ...CxT, typename Event>
    struct completions_has_event<completions<CxH,CxT...>, Event> {
      static constexpr bool value =
        std::is_same<Event, typename CxH::event_t>::value ||
        completions_has_event<completions<CxT...>, Event>::value;
    };
  }

  //////////////////////////////////////////////////////////////////////////////
  // detail::completions_is_event_sync: detects if there exists a buffered_cx or
  // blocking_cx action tagged by the given event in the completions list

  namespace detail {
    template<typename Cxs, typename Event>
    struct completions_is_event_sync;
    
    template<typename Event>
    struct completions_is_event_sync<completions<>, Event> {
      static constexpr bool value = false;
    };
    template<typename ...CxT, typename Event>
    struct completions_is_event_sync<completions<buffered_cx<Event>,CxT...>, Event> {
      static constexpr bool value = true;
    };
    template<typename ...CxT, typename Event>
    struct completions_is_event_sync<completions<blocking_cx<Event>,CxT...>, Event> {
      static constexpr bool value = true;
    };
    template<typename CxH, typename ...CxT, typename Event>
    struct completions_is_event_sync<completions<CxH,CxT...>, Event> {
      static constexpr bool value =
        completions_is_event_sync<completions<CxT...>, Event>::value;
    };
  }
  
  //////////////////////////////////////////////////////////////////////
  // detail::is_not_array

  namespace detail {
    template<typename T>
    struct is_not_array :
      std::integral_constant<
          bool,
          !std::is_array<typename std::remove_reference<T>::type>::value
        > {};
  }

  //////////////////////////////////////////////////////////////////////
  // detail::as_rpc_return: computes the return type of as_rpc, while
  // also checking whether the function object can be invoked with the
  // given arguments when converted to their off-wire types

  namespace detail {
    template<typename Call, typename = void>
    struct check_rpc_call : std::false_type {};
    template<typename Fn, typename ...Args>
    struct check_rpc_call<
        Fn(Args...),
        decltype(
          std::declval<typename binding<Fn>::off_wire_type&&>()(
            std::declval<typename binding<Args>::off_wire_type&&>()...
          ),
          void()
        )
      > : std::true_type {};

    template<typename Event, typename Fn, typename ...Args>
    struct as_rpc_return {
        static_assert(
          detail::trait_forall<
              detail::is_not_array,
              Args...
            >::value,
          "Arrays may not be passed as arguments to as_rpc. "
          "To send the contents of an array, use upcxx::make_view() to construct a upcxx::view over the elements."
        );
        static_assert(
          detail::trait_forall<
              is_serializable,
              typename binding<Args>::on_wire_type...
            >::value,
          "All rpc arguments must be Serializable."
        );
        static_assert(
          check_rpc_call<Fn(Args...)>::value,
          "function object provided to as_rpc cannot be invoked on the given arguments as rvalue references "
          "(after deserialization of the function object and arguments). "
          "Note: make sure that the function object does not have any non-const lvalue-reference parameters."
        );
        static_assert(
          detail::trait_forall<
              detail::type_respects_static_size_limit,
              typename binding<Args>::on_wire_type...
            >::value,
          UPCXX_STATIC_ASSERT_RPC_MSG(remote_cx::as_rpc)
        );

      using type = completions<
          rpc_cx<Event, typename bind<Fn&&, Args&&...>::return_type>
        >;
    };
  }

  //////////////////////////////////////////////////////////////////////////////
  // User-interface for obtaining a singleton completion list (one action tied
  // to one event).

  namespace detail {
    template<typename Event>
    struct support_as_future {
      static constexpr completions<future_cx<Event>> as_future() {
        return {future_cx<Event>{}};
      }
    };

    template<typename Event>
    struct support_as_promise {
      template<typename ...T>
      static constexpr completions<promise_cx<Event, T...>> as_promise(promise<T...> pro) {
        return {promise_cx<Event, T...>{
          static_cast<promise_shref<T...>&&>(promise_as_shref(pro))
        }};
      }
    };

    template<typename Event>
    struct support_as_lpc {
      template<typename Fn>
      static constexpr completions<lpc_cx<Event, typename std::decay<Fn>::type>>
      as_lpc(persona &target, Fn &&func) {
        return {
          lpc_cx<Event, typename std::decay<Fn>::type>{target, std::forward<Fn>(func)}
        };
      }
    };

    template<typename Event>
    struct support_as_buffered {
      static constexpr completions<buffered_cx<Event>> as_buffered() {
        return {buffered_cx<Event>{}};
      }
    };

    template<typename Event>
    struct support_as_blocking {
      static constexpr completions<blocking_cx<Event>> as_blocking() {
        return {blocking_cx<Event>{}};
      }
    };

    template<typename Event>
    struct support_as_rpc {
      template<typename Fn, typename ...Args>
      static typename detail::as_rpc_return<Event, Fn, Args...>::type
      as_rpc(Fn &&fn, Args &&...args) {
        return {
          rpc_cx<Event, typename detail::bind<Fn&&, Args&&...>::return_type>{
            upcxx::bind(std::forward<Fn>(fn), std::forward<Args>(args)...)
          }
        };
      }
    };
  }

  struct source_cx:
    detail::support_as_blocking<source_cx_event>,
    detail::support_as_buffered<source_cx_event>,
    detail::support_as_future<source_cx_event>,
    detail::support_as_lpc<source_cx_event>,
    detail::support_as_promise<source_cx_event> {};
  
  struct operation_cx:
    detail::support_as_blocking<operation_cx_event>,
    detail::support_as_future<operation_cx_event>,
    detail::support_as_lpc<operation_cx_event>,
    detail::support_as_promise<operation_cx_event> {};
  
  struct remote_cx:
    detail::support_as_rpc<remote_cx_event> {};

  //////////////////////////////////////////////////////////////////////
  // cx_non_future_return and cx_result_combine: Collect results of
  // as_rpc invocations. The purpose is to ensure that returned
  // futures are propagated all the way out to the top-level binding,
  // so that view-buffer and input-argument lifetime extension can be
  // applied.

  namespace detail {
    struct cx_non_future_return {};

    // collect futures
    template<typename T1, typename T2>
    cx_non_future_return cx_result_combine(T1 &&v1, T2 &&v2) {
      return cx_non_future_return{};
    }
    template<typename T1, typename Kind2, typename ...T2>
    future1<Kind2, T2...> cx_result_combine(T1 &&v1,
                                            future1<Kind2, T2...> &&v2) {
      return std::forward<future1<Kind2, T2...>>(v2);
    }
    template<typename Kind1, typename ...T1, typename T2>
    future1<Kind1, T1...> cx_result_combine(future1<Kind1, T1...> &&v1,
                                            T2 &&v2) {
      return std::forward<future1<Kind1, T1...>>(v1);
    }
    template<typename Kind1, typename ...T1, typename Kind2, typename ...T2>
    auto cx_result_combine(future1<Kind1, T1...> &&v1,
                           future1<Kind2, T2...> &&v2)
      UPCXX_RETURN_DECLTYPE(
        detail::when_all_fast(std::forward<future1<Kind1, T1...>>(v1),
                              std::forward<future1<Kind2, T2...>>(v2))
      ) {
      return detail::when_all_fast(std::forward<future1<Kind1, T1...>>(v1),
                                   std::forward<future1<Kind2, T2...>>(v2));
    }
  }


  //////////////////////////////////////////////////////////////////////////////
  /* detail::cx_state: Per action state that survives until the event is
  triggered. For future_cx's this holds a promise instance which seeds the
  future given back to the user. All other cx actions get their information
  stored as-is.

  // Specializations should look like:
  template<typename Event, typename ...T>
  struct cx_state<whatever_cx<Event>, std::tuple<T...>> {
    // There will be exatcly one call to one of the following functions before
    // this state destructs...

    void operator()(T...) {
      // This functions should be marked `&&` but isn't to support legacy.
      
      // Event has been satisfied so fire this action, Must work in any progress
      // context. Notice event values are taken sans-reference since an event
      // may have multiple "listeners", each should get a private copy.
    }
    
    lpc_dormant<T...> to_lpc_dormant(lpc_dormant<T...> *tail) && {
      // Convert this state into a dormant lpc (chained against a supplied tail
      // which could be null).
    }
  };
  */
  
  namespace detail {
    template<typename Cx /* the action specialized upon */,
             typename EventArgsTup /* tuple containing list of action's value types*/>
    struct cx_state;
    
    template<typename Event>
    struct cx_state<buffered_cx<Event>, std::tuple<>> {
      cx_state(buffered_cx<Event>) {}
      void operator()() {}
    };
    
    template<typename Event>
    struct cx_state<blocking_cx<Event>, std::tuple<>> {
      cx_state(blocking_cx<Event>) {}
      void operator()() {}
    };
    
    template<typename Event, progress_level level, typename ...T>
    struct cx_state<future_cx<Event,level>, std::tuple<T...>> {
      future_header_promise<T...> *pro_; // holds ref, no need to drop it in destructor since we move out it in either operator() ro to_lpc_dormant

      cx_state(future_cx<Event,level>):
        pro_(new future_header_promise<T...>) {
      }

      // completions_returner_head handles cx_state<future_cx> specially and requires
      // this additional method.
      future<T...> get_future() const {
        return detail::promise_get_future(pro_);
      }
      
      lpc_dormant<T...>* to_lpc_dormant(lpc_dormant<T...> *tail) && {
        return detail::make_lpc_dormant_quiesced_promise<T...>(
          upcxx::current_persona(), progress_level::user, /*move ref*/pro_, tail
        );
      }
      
      void operator()(T ...vals) {
        backend::fulfill_during<level>(
          /*move ref*/pro_, std::tuple<T...>(static_cast<T&&>(vals)...)
        );
      }
    };

    /* There are multiple specializations for promise_cx since both the promise
    and event have their own `T...` and either these must match or the event's
    list is empty */
    
    // Case when promise and event have matching (non-empty) type lists T...
    template<typename Event, typename ...T>
    struct cx_state<promise_cx<Event,T...>, std::tuple<T...>> {
      future_header_promise<T...> *pro_; // holds ref

      cx_state(promise_cx<Event,T...> &&cx):
        pro_(static_cast<promise_cx<Event,T...>&&>(cx).pro_.steal_header()) {
        detail::promise_require_anonymous(pro_, 1);
      }

      lpc_dormant<T...>* to_lpc_dormant(lpc_dormant<T...> *tail) && {
        future_header_promise<T...> *pro = /*move ref*/pro_;
        return detail::make_lpc_dormant(
          upcxx::current_persona(), progress_level::user,
          [/*move ref*/pro](T &&...results) {
            backend::fulfill_during<progress_level::user>(
              /*move ref*/pro, std::tuple<T...>(static_cast<T&&>(results)...)
            );
          },
          tail
        );
      }
      
      void operator()(T ...vals) {
        backend::fulfill_during<progress_level::user>(
          /*move ref*/pro_, std::tuple<T...>(static_cast<T&&>(vals)...)
        );
      }
    };
    // Case when event type list is empty
    template<typename Event, typename ...T>
    struct cx_state<promise_cx<Event,T...>, std::tuple<>> {
      future_header_promise<T...> *pro_; // holds ref

      cx_state(promise_cx<Event,T...> &&cx):
        pro_(static_cast<promise_cx<Event,T...>&&>(cx).pro_.steal_header()) {
        detail::promise_require_anonymous(pro_, 1);
      }
      
      lpc_dormant<>* to_lpc_dormant(lpc_dormant<> *tail) && {
        future_header_promise<T...> *pro = /*move ref*/pro_;
        return detail::make_lpc_dormant(
          upcxx::current_persona(), progress_level::user,
          [/*move ref*/pro]() {
            backend::fulfill_during<progress_level::user>(/*move ref*/pro, 1);
          },
          tail
        );
      }
      
      void operator()() {
        backend::fulfill_during<progress_level::user>(/*move ref*/pro_, 1);
      }
    };
    // Case when promise and event type list are both empty
    template<typename Event>
    struct cx_state<promise_cx<Event>, std::tuple<>> {
      future_header_promise<> *pro_; // holds ref

      cx_state(promise_cx<Event> &&cx):
        pro_(static_cast<promise_cx<Event>&&>(cx).pro_.steal_header()) {
        detail::promise_require_anonymous(pro_, 1);
      }

      lpc_dormant<>* to_lpc_dormant(lpc_dormant<> *tail) && {
        future_header_promise<> *pro = /*move ref*/pro_;
        return detail::make_lpc_dormant<>(
          upcxx::current_persona(), progress_level::user,
          [/*move ref*/pro]() {
            backend::fulfill_during<progress_level::user>(/*move ref*/pro, 1);
          },
          tail
        );
      }
      
      void operator()() {
        backend::fulfill_during<progress_level::user>(/*move ref*/pro_, 1);
      }
    };
    
    template<typename Event, typename Fn, typename ...T>
    struct cx_state<lpc_cx<Event,Fn>, std::tuple<T...>> {
      persona *target_;
      Fn fn_;
      
      cx_state(lpc_cx<Event,Fn> &&cx):
        target_(cx.target_),
        fn_(static_cast<Fn&&>(cx.fn_)) {
        upcxx::current_persona().undischarged_n_ += 1;
      }

      lpc_dormant<T...>* to_lpc_dormant(lpc_dormant<T...> *tail) && {
        upcxx::current_persona().undischarged_n_ -= 1;
        return detail::make_lpc_dormant(*target_, progress_level::user, std::move(fn_), tail);
      }
      
      void operator()(T ...vals) {
        target_->lpc_ff(
          detail::lpc_bind<Fn,T...>(static_cast<Fn&&>(fn_), static_cast<T&&>(vals)...)
        );
        upcxx::current_persona().undischarged_n_ -= 1;
      }
    };

    // cx_state<rpc_cx<...>> does not fit the usual mold since the event isn't
    // triggered locally.
    template<typename Event, typename Fn, typename ...T>
    struct cx_state<rpc_cx<Event,Fn>, std::tuple<T...>> {
      Fn fn_;
      
      cx_state(rpc_cx<Event,Fn> &&cx):
        fn_(static_cast<Fn&&>(cx.fn_)) {
      }
      
      typename std::result_of<Fn&&(T&&...)>::type
      operator()(T ...vals) {
        return static_cast<Fn&&>(fn_)(static_cast<T&&>(vals)...);
      }
    };
  }

  //////////////////////////////////////////////////////////////////////////////
  /* detail::completions_state: Constructed against a user-supplied
  completions<...> value, converting its action descriptors into tracked
  state (e.g. what_cx<Event> becomes cx_state<what_cx<Event>>). Clients are typically
  given a completions<...> by the user (a list of actions), and should
  construct this class to convert those actions into a list of tracked stateful
  things. As various events complete, this thing should be notified and it will
  visit all its members firing their actions.

  A completions_state takes an EventPredicate to select which events are
  tracked (the unselected carry no state and do nothing when notified). This is
  required to support remote events. When both local and remote events are in
  play the client will construct two completions_state's against the same
  completions<...> list. One instance to track local completions (using
  EventPredicate=detail::event_is_here), and the other (with
  EventPredicate=detail::event_is_remote) to be shipped off and invoked
  remotely.

  We also take an EventValues map which assigns to each event type the runtime
  value types produced by the event (some T...).

  More specifically the template args are...
  
  EventPredicate<Event>::value: Maps an event-type to a compile-time bool value
  for enabling that event in this instance.

  EventValues::tuple_t<Event>: Maps an event-type to a type-list (wrapped as a
  tuple<T...>) which types the values reported by the completed action.
  `operator()` will expect that the runtime values it receives match the types
  reported by this map for the given event.
  
  ordinal: indexes the nesting depth of this type so that base classes
  with identical types can be disambiguated.
  */
  namespace detail {
    template<template<typename> class EventPredicate,
             typename EventValues,
             typename Cxs,
             int ordinal=0> 
    struct completions_state /*{
      using completions_t = Cxs; // retrieve the original completions<...> user type

      // True iff no events contained in `Cxs` are enabled by `EventPredicate`.
      static constexpr bool empty;

      // Fire actions corresponding to `Event` if its enabled. Type-list
      // V... should match the T... in `EventValues::tuple_t<Event>`.
      template<typename Event, typename ...V>
      void operator()(V&&...); // should be &&, but not for legacy

      // Create a callable to fire all actions associated with given event. Note
      // this method consumes the instance (&&) so Event should be the only event
      // enabled by our predicate if any.
      template<typename Event>
      SomeCallable bind_event() &&;

      // Convert states of actions associated with given Event to dormant lpc list
      template<typename Event>
      lpc_dormant<...> to_lpc_dormant() &&;
    }*/;

    // completions_state specialization for empty completions<>
    template<template<typename> class EventPredicate,
             typename EventValues,
             int ordinal>
    struct completions_state<EventPredicate, EventValues,
                             completions<>, ordinal> {

      using completions_t = completions<>;
      static constexpr bool empty = true;
      
      completions_state(completions<>) {}
      
      template<typename Event, typename ...V>
      void operator()(V &&...vals) {/*nop*/}

      struct event_bound {
        template<typename ...V>
        void operator()(V &&...vals) {/*nop*/}
      };
      
      template<typename Event>
      event_bound bind_event() && {
        return event_bound{};
      }

      template<typename Event>
      typename detail::tuple_types_into<
          typename EventValues::template tuple_t<Event>,
          lpc_dormant
        >::type*
      to_lpc_dormant() && {
        return nullptr; // the empty lpc_dormant list
      }
    };

    /* completions_state for non-empty completions<...> deconstructs list one
    at a time recursively. The first element is the head, the list of
    everything else is the tail. It inherits a completions_state_head for each
    list item, passing it the predicate boolean evaluated for the item's event,
    and the entire EventValues mapping (not sure why as this feels like the
    right place to evaluate the mapping on the item's event type just like we
    do with the predicate).

    completions_state_head handles the logic of being and doing nothing for
    disabled events. It also exposes action firing mechanisms that test that the
    completed event matches the one associated with the action. This makes the
    job of completions_state easy, as it can just visit all the heads and fire
    them.
    */

    template<bool event_selected, typename EventValues, typename Cx, int ordinal>
    struct completions_state_head;

    // completions_state_head with event disabled (predicate=false)
    template<typename EventValues, typename Cx, int ordinal>
    struct completions_state_head<
        /*event_enabled=*/false, EventValues, Cx, ordinal
      > {
      static constexpr bool empty = true;

      completions_state_head(Cx &&cx) {}
      
      template<typename Event, typename ...V>
      void operator()(V&&...) {/*nop*/}
    };

    template<typename Cx>
    using cx_event_t = typename Cx::event_t;

    // completions_state_head with event enabled (predicate=true)
    template<typename EventValues, typename Cx, int ordinal>
    struct completions_state_head<
        /*event_enabled=*/true, EventValues, Cx, ordinal
      > {
      static constexpr bool empty = false;

      cx_state<Cx, typename EventValues::template tuple_t<cx_event_t<Cx>>> state_;
      
      completions_state_head(Cx &&cx):
        state_(std::move(cx)) {
      }
      
      template<typename ...V>
      auto operator_case(std::integral_constant<bool,true>, V &&...vals)
        UPCXX_RETURN_DECLTYPE(
          state_.operator()(std::forward<V>(vals)...)
        ) {
        // Event matches CxH::event_t
        return state_.operator()(std::forward<V>(vals)...);
      }
      template<typename ...V>
      void operator_case(std::integral_constant<bool,false>, V &&...vals) {
        // Event mismatch = nop
      }

      // fire state if Event == CxH::event_t
      template<typename Event, typename ...V>
      auto operator()(V &&...vals)
        UPCXX_RETURN_DECLTYPE(
          this->operator_case(
            std::integral_constant<
              bool,
              std::is_same<Event, typename Cx::event_t>::value
            >{},
            std::forward<V>(vals)...
          )
        ) {
        return this->operator_case(
          std::integral_constant<
            bool,
            std::is_same<Event, typename Cx::event_t>::value
          >{},
          std::forward<V>(vals)...
        );
      }
      
      template<typename Event, typename Lpc>
      Lpc* to_lpc_dormant_case(std::true_type, Lpc *tail) && {
        return std::move(state_).to_lpc_dormant(tail);
      }

      template<typename Event, typename Lpc>
      Lpc* to_lpc_dormant_case(std::false_type, Lpc *tail) && {
        return tail; // ignore this event, just return tail (meaning no append)
      }

      // Append dormant lpc to tail iff Event == CxH::event_t
      template<typename Event, typename Lpc>
      Lpc* to_lpc_dormant(Lpc *tail) && {
        return std::move(*this).template to_lpc_dormant_case<Event>(
          std::integral_constant<bool,
            std::is_same<Event, typename Cx::event_t>::value
          >(),
          tail
        );
      }
    };

    // Now we can define completions_state for non empty completions<...>
    template<template<typename> class EventPredicate,
             typename EventValues, typename CxH, typename ...CxT,
             int ordinal>
    struct completions_state<EventPredicate, EventValues,
                             completions<CxH,CxT...>, ordinal>:
        // head base class
        completions_state_head<EventPredicate<typename CxH::event_t>::value,
                               EventValues, CxH, ordinal>,
        // Tail base class. Incrementing the ordinal is essential so that the
        // head bases of this tail base are disambiguated from our head.
        completions_state<EventPredicate, EventValues,
                          completions<CxT...>, ordinal+1> {

      using completions_t = completions<CxH, CxT...>;
      
      using head_t = completions_state_head<
          /*event_enabled=*/EventPredicate<typename CxH::event_t>::value,
          EventValues, CxH, ordinal
        >;
      using tail_t = completions_state<EventPredicate, EventValues,
                                       completions<CxT...>, ordinal+1>;

      static constexpr bool empty = head_t::empty && tail_t::empty;
      
      completions_state(completions<CxH,CxT...> &&cxs):
        head_t(cxs.head_moved()),
        tail_t(cxs.tail_moved()) {
      }
      completions_state(head_t &&head, tail_t &&tail):
        head_t(std::move(head)),
        tail_t(std::move(tail)) {
      }
      
      head_t& head() { return static_cast<head_t&>(*this); }
      head_t const& head() const { return *this; }
      
      tail_t& tail() { return static_cast<tail_t&>(*this); }
      tail_t const& tail() const { return *this; }

      // return type of firing off the given completions_state
      template<typename Event, typename CS, bool do_move,
               typename ...V>
      using rettype = decltype(
        std::declval<CS&>().template operator()<Event>(
          std::declval<typename std::conditional<do_move, V&&, V const&>::type>()...
        )
      );
      // converted return type (used for making PGI happy)
      template<typename Event, typename CS, bool do_move,
               typename ...V>
      using converted_rettype = typename std::conditional<
        detail::is_future1<
          typename std::decay<rettype<Event, CS, do_move, V...>>::type
        >::value,
        rettype<Event, CS, do_move, V...>,
        cx_non_future_return
      >::type;

      // fire off a completions_state, preserving the future return
      template<typename Event, typename CSOut, bool do_move,
               typename CSIn, typename ...V>
      static typename std::enable_if<
        detail::is_future1<
          typename std::decay<rettype<Event, CSOut, do_move, V...>>::type
        >::value,
        rettype<Event, CSOut, do_move, V...>
      >::type
      fire(CSIn &&me, V &&...vals) {
        return actually_fire<Event, CSOut, do_move>(
          static_cast<CSIn&&>(me), static_cast<V&&>(vals)...
        );
      }

      // fire off a completions_state, converting non-future return
      // type to cx_non_future_return
      // The main purpose of this is actually to deal with a void
      // return type. We need a return value so that it can be passed
      // to cx_result_combine(). Since the return type is not a
      // future, we don't care about the return value in the non-void
      // case and just return a cx_non_future_return unconditionally.
      template<typename Event, typename CSOut, bool do_move,
               typename CSIn, typename ...V>
      static typename std::enable_if<
        !detail::is_future1<
          typename std::decay<rettype<Event, CSOut, do_move, V...>>::type
        >::value,
        cx_non_future_return
      >::type
      fire(CSIn &&me, V &&...vals) {
        actually_fire<Event, CSOut, do_move>(
          static_cast<CSIn&&>(me), static_cast<V&&>(vals)...
        );
        return cx_non_future_return{};
      }

      template<typename Event, typename CSOut, bool do_move,
               typename CSIn, typename ...V>
      static rettype<Event, CSOut, do_move, V...>
      actually_fire(CSIn &&me, V &&...vals) {
        return static_cast<CSOut&>(me).template operator()<Event>(
          static_cast<
              // An empty tail means we are the lucky one who gets the
              // opportunity to move-out the given values (if caller supplied
              // reference type permits, thank you reference collapsing).
              typename std::conditional<do_move, V&&, V const&>::type
            >(vals)...
        );
      }

      template<typename Event, typename ...V>
      auto operator()(V &&...vals)
        UPCXX_RETURN_DECLTYPE(
          cx_result_combine(
            std::declval<converted_rettype<Event, head_t, tail_t::empty, V...>>(),
            std::declval<converted_rettype<Event, tail_t, false, V...>>()
          )
        ) {
        return cx_result_combine(
          // fire the head element
          fire<Event, head_t, tail_t::empty>(*this, static_cast<V&&>(vals)...),
          // recurse to fire remaining elements
          fire<Event, tail_t, false>(*this, static_cast<V&&>(vals)...)
        );
      }

      template<typename Event>
      struct event_bound {
        using dtype = deserialized_type_t<completions_state>;

        template<typename ...V>
        auto operator()(dtype &&me, V &&...vals)
          UPCXX_RETURN_DECLTYPE(
            cx_result_combine(
              std::declval<converted_rettype<Event, typename dtype::head_t, true, V...>>(),
              std::declval<converted_rettype<Event, typename dtype::tail_t, true, V...>>()
            )
          ) {
          return cx_result_combine(
            // fire the head element
            fire<Event, typename dtype::head_t, true>(std::forward<dtype>(me),
                                                      std::forward<V>(vals)...),
            // recurse to fire remaining elements
            fire<Event, typename dtype::tail_t, true>(std::forward<dtype>(me),
                                                      std::forward<V>(vals)...)
          );
        }
      };
      
      template<typename Event>
      typename detail::template bind<event_bound<Event>, completions_state>::return_type
      bind_event() && {
        /* This is gross. We are moving our entire instance into this bound
        callable instead of just the items related to Event. This limits the
        applicability of bind_event to completion_state's with only a single
        enabled event type. Fortunately thats all that is needed since
        bind_event is currently only used to produce the callable that is
        shipped as an rpc.
        TODO: we should at least assert no events besides Event are enabled by
        our predicate.
        */
        return upcxx::bind(event_bound<Event>(), std::move(*this));
      }

      template<typename Event>
      typename detail::tuple_types_into<
          typename EventValues::template tuple_t<Event>,
          lpc_dormant
        >::type*
      to_lpc_dormant() && {
        return static_cast<head_t&&>(*this).template to_lpc_dormant<Event>(
          static_cast<tail_t&&>(*this).template to_lpc_dormant<Event>()
        );
      }
    };
  }

  //////////////////////////////////////////////////////////////////////
  // Serialization of a completions_state of rpc_cx's

  /* TODO: I believe enabling serialization for completions_state is totally
  unnecessary. Now that we have completions_state::bind_event, anyone wanting
  to send this over the wire should just send my_cx_state.bind_event<rpc_cx_event>()
  instead.
  */
  
  template<typename EventValues, typename Event, typename Fn, int ordinal>
  struct serialization<
      detail::completions_state_head<
        /*event_enabled=*/true, EventValues, rpc_cx<Event,Fn>, ordinal
      >
    > {
    using type = detail::completions_state_head<true, EventValues, rpc_cx<Event,Fn>, ordinal>;

    static constexpr bool is_serializable = serialization_traits<Fn>::is_serializable;
    
    template<typename Ub>
    static auto ubound(Ub ub, type const &s)
      UPCXX_RETURN_DECLTYPE(
        ub.template cat_ubound_of<Fn>(s.state_.fn_)
      ) {
      return ub.template cat_ubound_of<Fn>(s.state_.fn_);
    }

    template<typename Writer>
    static void serialize(Writer &w, type const &s) {
      w.template write<Fn>(s.state_.fn_);
    }

    using deserialized_type = detail::completions_state_head<
        true, EventValues,
        rpc_cx<Event, typename serialization_traits<Fn>::deserialized_type>,
        ordinal
      >;
    
    static constexpr bool skip_is_fast = serialization_traits<Fn>::skip_is_fast;
    static constexpr bool references_buffer = serialization_traits<Fn>::references_buffer;
    
    template<typename Reader>
    static void skip(Reader &r) {
      r.template skip<Fn>();
    }

    template<typename Reader>
    static deserialized_type* deserialize(Reader &r, void *spot) {
      return new(spot) deserialized_type(r.template read<Fn>());
    }
  };

  template<typename EventValues, typename Cx, int ordinal>
  struct serialization<
      detail::completions_state_head</*event_enabled=*/false, EventValues, Cx, ordinal>
    >:
    detail::serialization_trivial<
      detail::completions_state_head</*event_enabled=*/false, EventValues, Cx, ordinal>,
      /*empty=*/true
    > {
    static constexpr bool is_serializable = true;
  };
  
  template<template<typename> class EventPredicate,
           typename EventValues, int ordinal>
  struct serialization<
      detail::completions_state<EventPredicate, EventValues, completions<>, ordinal>
    >:
    detail::serialization_trivial<
      detail::completions_state<EventPredicate, EventValues, completions<>, ordinal>,
      /*empty=*/true
    > {
    static constexpr bool is_serializable = true;
  };

  template<template<typename> class EventPredicate,
           typename EventValues, typename CxH, typename ...CxT, int ordinal>
  struct serialization<
      detail::completions_state<EventPredicate, EventValues, completions<CxH,CxT...>, ordinal>
    > {
    using type = detail::completions_state<EventPredicate, EventValues, completions<CxH,CxT...>, ordinal>;

    static constexpr bool is_serializable =
      serialization_traits<typename type::head_t>::is_serializable &&
      serialization_traits<typename type::tail_t>::is_serializable;
    
    template<typename Ub>
    static auto ubound(Ub ub, type const &cxs)
      UPCXX_RETURN_DECLTYPE(
        ub.template cat_ubound_of<typename type::head_t>(cxs.head())
          .template cat_ubound_of<typename type::tail_t>(cxs.tail())
      ) {
      return ub.template cat_ubound_of<typename type::head_t>(cxs.head())
               .template cat_ubound_of<typename type::tail_t>(cxs.tail());
    }

    template<typename Writer>
    static void serialize(Writer &w, type const &cxs) {
      w.template write<typename type::head_t>(cxs.head());
      w.template write<typename type::tail_t>(cxs.tail());
    }

    using deserialized_type = detail::completions_state<
        EventPredicate, EventValues,
        completions<typename CxH::deserialized_cx,
                    typename CxT::deserialized_cx...>,
        ordinal
      >;

    static constexpr bool skip_is_fast =
      serialization_traits<typename type::head_t>::skip_is_fast &&
      serialization_traits<typename type::tail_t>::skip_is_fast;
    static constexpr bool references_buffer =
      serialization_traits<typename type::head_t>::references_buffer ||
      serialization_traits<typename type::tail_t>::references_buffer;

    template<typename Reader>
    static void skip(Reader &r) {
      r.template skip<typename type::head_t>();
      r.template skip<typename type::tail_t>();
    }

    template<typename Reader>
    static deserialized_type* deserialize(Reader &r, void *spot) {
      auto h = r.template read<typename type::head_t>();
      auto t = r.template read<typename type::tail_t>();
      return new(spot) deserialized_type(std::move(h), std::move(t));
    }
  };

  //////////////////////////////////////////////////////////////////////////////
  /* detail::completions_returner: Manage return type for completions<...>
  object. Construct one of these instances against a detail::completions_state&
  *before* any injection is done. Afterwards, call operator() to produce the
  return value desired by user.
  */
  
  namespace detail {
    /* The implementation of completions_returner tears apart the completions<...>
    list matching for future_cx's since those are the only ones that return
    something to user.
    */
    template<template<typename> class EventPredicate,
             typename EventValues, typename Cxs>
    struct completions_returner;

    // completions_returner for empty completions<>
    template<template<typename> class EventPredicate,
             typename EventValues>
    struct completions_returner<EventPredicate, EventValues, completions<>> {
      using return_t = void;

      template<int ordinal>
      completions_returner(
          completions_state<EventPredicate, EventValues, completions<>, ordinal>&
        ) {
      }
      
      void operator()() const {/*return void*/}
    };

    // completions_returner_head is inherited by completions_returner to dismantle
    // the head element of the list and recursively inherit completions_returner
    // of the tail.
    template<template<typename> class EventPredicate,
             typename EventValues, typename Cxs/*user completions<...>*/,
             typename TailReturn/*return type computed by tail*/>
    struct completions_returner_head;

    // specialization: we found a future_cx and are appending our return value
    // onto a tuple of return values
    template<template<typename> class EventPredicate,
             typename EventValues, typename CxH_event, progress_level level, typename ...CxT,
             typename ...TailReturn_tuplees>
    struct completions_returner_head<
        EventPredicate, EventValues,
        completions<future_cx<CxH_event,level>, CxT...>,
        std::tuple<TailReturn_tuplees...>
      > {
      
      using return_t = std::tuple<
          detail::future_from_tuple_t<
            detail::future_kind_default,
            typename EventValues::template tuple_t<CxH_event>
          >,
          TailReturn_tuplees...
        >;

      return_t ans_;
      return_t&& operator()() { return std::move(ans_); }
      
      template<typename CxState, typename Tail>
      completions_returner_head(CxState &s, Tail &&tail):
        ans_{
          std::tuple_cat(
            std::make_tuple(s.head().state_.get_future()),
            tail()
          )
        } {
      }
    };

    // specialization: we found a future_cx and one other item is returning a
    // value, so we introduce a two-element tuple.
    template<template<typename> class EventPredicate,
             typename EventValues, typename CxH_event, progress_level level, typename ...CxT,
             typename TailReturn_not_tuple>
    struct completions_returner_head<
        EventPredicate, EventValues,
        completions<future_cx<CxH_event,level>, CxT...>,
        TailReturn_not_tuple
      > {
      
      using return_t = std::tuple<
          detail::future_from_tuple_t<
            detail::future_kind_default,
            typename EventValues::template tuple_t<CxH_event>
          >,
          TailReturn_not_tuple
        >;
      
      return_t ans_;
      return_t&& operator()() { return std::move(ans_); }
      
      template<typename CxState, typename Tail>
      completions_returner_head(CxState &s, Tail &&tail):
        ans_(
          std::make_tuple(
            s.head().state_.get_future(),
            tail()
          )
        ) {
      }
    };

    // specialization: we found a future_cx and are the first to want to return
    // a value.
    template<template<typename> class EventPredicate,
             typename EventValues, typename CxH_event, progress_level level, typename ...CxT>
    struct completions_returner_head<
        EventPredicate, EventValues,
        completions<future_cx<CxH_event,level>, CxT...>,
        void
      > {
      
      using return_t = detail::future_from_tuple_t<
          detail::future_kind_default,
          typename EventValues::template tuple_t<CxH_event>
        >;
      
      return_t ans_;
      return_t&& operator()() { return std::move(ans_); }
      
      template<typename CxState, typename Tail>
      completions_returner_head(CxState &s, Tail&&):
        ans_(
          s.head().state_.get_future()
        ) {
      }
    };

    // specialization: the action is not a future, do not muck with return value
    template<template<typename> class EventPredicate,
             typename EventValues, typename CxH_not_future, typename ...CxT,
             typename TailReturn>
    struct completions_returner_head<
        EventPredicate, EventValues,
        completions<CxH_not_future, CxT...>,
        TailReturn
      >:
      completions_returner<
          EventPredicate, EventValues, completions<CxT...>
        > {
      
      template<typename CxState>
      completions_returner_head(
          CxState&,
          completions_returner<
            EventPredicate, EventValues, completions<CxT...>
          > &&tail
        ):
        completions_returner<
            EventPredicate, EventValues, completions<CxT...>
          >{std::move(tail)} {
      }
    };

    // completions_returner for non-empty completions<...>: inherit
    // completions_returner_head which will dismantle the head element and
    // recursively inherit completions_returner on the tail list.
    template<template<typename> class EventPredicate,
             typename EventValues, typename CxH, typename ...CxT>
    struct completions_returner<EventPredicate, EventValues,
                                completions<CxH,CxT...>>:
      completions_returner_head<
        EventPredicate, EventValues,
        completions<CxH,CxT...>,
        typename completions_returner<
            EventPredicate, EventValues, completions<CxT...>
          >::return_t
      > {

      template<int ordinal>
      completions_returner(
          completions_state<
              EventPredicate, EventValues, completions<CxH,CxT...>, ordinal
            > &s
        ):
        completions_returner_head<
          EventPredicate, EventValues,
          completions<CxH,CxT...>,
          typename completions_returner<
              EventPredicate, EventValues, completions<CxT...>
            >::return_t
        >{s,
          completions_returner<
              EventPredicate, EventValues, completions<CxT...>
            >{s.tail()}
        } {
      }
    };
  }
}
#endif

