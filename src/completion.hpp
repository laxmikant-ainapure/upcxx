#ifndef _96368972_b5ed_4e48_ac4f_8c868279e3dd
#define _96368972_b5ed_4e48_ac4f_8c868279e3dd

#include <upcxx/backend.hpp>
#include <upcxx/bind.hpp>
#include <upcxx/future.hpp>
#include <upcxx/persona.hpp>
#include <upcxx/utility.hpp>

#include <tuple>

namespace upcxx {
  //////////////////////////////////////////////////////////////////////
  // Event names for common completion events as used by rput/rget etc.
  // This set is extensible from anywhere in the source.
  
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
    
  //////////////////////////////////////////////////////////////////////
  // Signalling actions tagged by the event they react to.

  // Future completion
  template<typename Event, progress_level level = progress_level::user>
  struct future_cx {
    using event_t = Event;
  };

  // Promise completion
  template<typename Event, typename ...T>
  struct promise_cx {
    using event_t = Event;
    promise<T...> &pro_;
  };

  // Synchronous completion via best-effort buffering
  template<typename Event>
  struct buffered_cx {
    using event_t = Event;
  };

  // Synchronous completion via blocking on network/peers
  template<typename Event>
  struct blocking_cx {
    using event_t = Event;
  };

  // LPC completion
  template<typename Event, typename Fn>
  struct lpc_cx {
    using event_t = Event;
    
    persona *target_;
    Fn fn_;

    lpc_cx(persona &target, Fn fn):
      target_(&target),
      fn_(std::move(fn)) {
    }
  };
  
  // RPC completion. Arguments are bound into fn_.
  template<typename Event, typename Fn>
  struct rpc_cx {
    using event_t = Event;
    
    Fn fn_;
    rpc_cx(Fn fn): fn_(std::move(fn)) {}
  };
  
  //////////////////////////////////////////////////////////////////////
  // completions<...>: A list of tagged completion actions. We use
  // lisp-like lists where the head is the first element and the tail
  // is the list of everything after.
  
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

  //////////////////////////////////////////////////////////////////////
  // operator "|": Concatenates two completions lists.
  
  template<typename ...B>
  constexpr completions<B...> operator|(
      completions<> a, completions<B...> b
    ) {
    return std::move(b);
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

  //////////////////////////////////////////////////////////////////////
  // detail::completions_has_event: detects if there exists an action
  // tagged by the given event in the completions list.

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

  //////////////////////////////////////////////////////////////////////
  // detail::completions_is_event_sync: detects if there exists a
  // buffered_cx or blocking_cx action tagged by the given event in the
  // completions list

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
  // User-interface for obtaining a completion tied to an event.

  namespace detail {
    template<typename Event>
    struct support_as_future {
      using as_future_t = completions<future_cx<Event>>;

      static constexpr as_future_t as_future() {
        return {future_cx<Event>{}};
      }
    };

    template<typename Event>
    struct support_as_promise {
      template<typename ...T>
      using as_promise_t = completions<promise_cx<Event, T...>>;

      template<typename ...T>
      static constexpr as_promise_t<T...> as_promise(promise<T...> &pro) {
        return {promise_cx<Event, T...>{pro}};
      }
    };

    template<typename Event>
    struct support_as_lpc {
      template<typename Fn>
      using as_lpc_t = completions<lpc_cx<Event, Fn>>;

      template<typename Fn>
      static constexpr as_lpc_t<Fn> as_lpc(persona &target, Fn func) {
        return {
          lpc_cx<Event, Fn>{target, static_cast<Fn&&>(func)}
        };
      }
    };

    template<typename Event>
    struct support_as_buffered {
      using as_buffered_t = completions<buffered_cx<Event>>;

      static constexpr as_buffered_t as_buffered() {
        return {buffered_cx<Event>{}};
      }
    };

    template<typename Event>
    struct support_as_blocking {
      using as_blocking_t = completions<blocking_cx<Event>>;

      static constexpr as_blocking_t as_blocking() {
        return {blocking_cx<Event>{}};
      }
    };

    template<typename Event>
    struct support_as_rpc {
      template<typename Fn, typename ...Args>
      using rpc_cx_t =
        rpc_cx<Event,
               decltype(upcxx::bind(std::declval<Fn>(),
                                    std::declval<Args>()...))>;

      template<typename Fn, typename ...Args>
      using as_rpc_t = completions<rpc_cx_t<Fn, Args...>>;
      
      template<typename Fn, typename ...Args>
      static as_rpc_t<Fn&&, Args&&...> as_rpc(Fn &&fn, Args &&...args) {
        return {
          rpc_cx_t<Fn&&, Args&&...>{
            upcxx::bind(static_cast<Fn&&>(fn), static_cast<Args&&>(args)...)
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
  // cx_state: Per action state that survives until the event
  // is triggered. For future_cx's this holds a promise instance which
  // seeds the future given back to the user. All other cx actions get
  // their information stored as-is. All of these expose `operator()(T...)`
  // which is used to "fire" the action  safely from any progress context.
  
  namespace detail {
    template<typename Cx /* the action */,
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
      promise<T...> pro_;
      
      cx_state(future_cx<Event,level>) {}
      
      void operator()(T ...vals) {
        backend::fulfill_during<level>(std::move(pro_), std::tuple<T...>(std::forward<T>(vals)...));
      }
    };

    // promise and events have matching (non-empty) types
    template<typename Event, typename ...T>
    struct cx_state<promise_cx<Event,T...>, std::tuple<T...>> {
      promise<T...> &pro_;

      cx_state(promise_cx<Event,T...> cx):
        pro_(cx.pro_) {
        pro_.require_anonymous(1);
      }
      
      void operator()(T ...vals) {
        backend::fulfill_during<progress_level::user>(pro_, std::tuple<T...>(std::forward<T>(vals)...));
      }
    };
    // event is empty
    template<typename Event, typename ...T>
    struct cx_state<promise_cx<Event,T...>, std::tuple<>> {
      promise<T...> &pro_;

      cx_state(promise_cx<Event,T...> cx):
        pro_(cx.pro_) {
        pro_.require_anonymous(1);
      }
      
      void operator()() {
        backend::fulfill_during<progress_level::user>(pro_, 1);
      }
    };
    // promise and event are empty
    template<typename Event>
    struct cx_state<promise_cx<Event>, std::tuple<>> {
      promise<> &pro_;

      cx_state(promise_cx<Event> cx):
        pro_(cx.pro_) {
        pro_.require_anonymous(1);
      }
      
      void operator()() {
        backend::fulfill_during<progress_level::user>(pro_, 1);
      }
    };
    
    template<typename Event, typename Fn, typename ...T>
    struct cx_state<lpc_cx<Event,Fn>, std::tuple<T...>> {
      persona *target_;
      Fn fn_;
      
      cx_state(lpc_cx<Event,Fn> cx):
        target_(cx.target_),
        fn_(std::move(cx.fn_)) {
      }
      
      void operator()(T ...vals) {
        target_->lpc_ff(
          std::bind(std::move(fn_), std::forward<T>(vals)...)
        );
      }
    };
    
    template<typename Event, typename Fn, typename ...T>
    struct cx_state<rpc_cx<Event,Fn>, std::tuple<T...>> {
      Fn fn_;
      
      cx_state(rpc_cx<Event,Fn> cx):
        fn_{std::move(cx.fn_)} {
      }
      
      auto operator()(T ...vals) ->
        decltype(fn_(std::forward<T>(vals)...)) {
        return fn_(std::forward<T>(vals)...);
      }
    };
  }

  //////////////////////////////////////////////////////////////////////
  // detail::completions_state: Constructed against a user-supplied
  // completions<...> value. This is what remembers the actions or
  // holds the promises (need by future returns) and so should probably
  // be heap allocated. To fire all actions registered to an event
  // call `operator()` with the event type as the first template arg.
  //
  // EventPredicate<Event>::value: Maps an event-type to a compile-time
  // bool value for enabling that event in this object. Events which
  // aren't enabled are not copied out of the constructor-time
  // completions<...> instance and execute no-ops under operator().
  //
  // EventValues::future_t<Event>: Maps an event-type to a type-list
  // (as future) which types the values reported by the completed
  // action. `operator()` will expect that the runtime values it receives
  // match the types reported by this map for the given event.
  namespace detail {
    template<template<typename> class EventPredicate,
             typename EventValues,
             typename Cxs>
    struct completions_state /*{
      using completions_t = Cxs;

      // True iff no events contained in `Cxs` are enabled by `EventPredicate`.
      static constexpr bool empty;

      // Fire actions corresponding to `Event` if its enabled. Type-list
      // V... should match the T... in `EventValues::future_t<Event>`.
      template<typename Event, typename ...V>
      void operator()(V&&...);
    }*/;
    
    template<template<typename> class EventPredicate,
             typename EventValues>
    struct completions_state<EventPredicate, EventValues,
                             completions<>> {

      using completions_t = completions<>;
      static constexpr bool empty = true;
      
      completions_state(completions<>) {}
      
      template<typename Event, typename ...V>
      void operator()(V &&...vals) {/*nop*/}
    };

    template<bool event_selected, typename EventValues, typename Cx>
    struct completions_state_head;
    
    template<typename EventValues,
             typename Cx>
    struct completions_state_head<
        /*event_enabled=*/false, EventValues, Cx
      > {
      static constexpr bool empty = true;

      completions_state_head(Cx cx) {}
      
      template<typename Event, typename ...V>
      void operator()(V&&...) {/*nop*/}
    };

    template<typename Cx>
    using cx_event_t = typename Cx::event_t;

    template<typename EventValues, typename Cx>
    struct completions_state_head<
        /*event_enabled=*/true, EventValues, Cx
      > {
      static constexpr bool empty = false;

      cx_state<Cx, typename EventValues::template tuple_t<cx_event_t<Cx>>> state_;
      
      completions_state_head(Cx cx):
        state_(std::move(cx)) {
      }
      
      template<typename ...V>
      void operator_case(std::integral_constant<bool,true>, V &&...vals) {
        // Event matches CxH::event_t
        state_.operator()(std::forward<V>(vals)...);
      }
      template<typename ...V>
      void operator_case(std::integral_constant<bool,false>, V &&...vals) {
        // Event mismatch = nop
      }

      // fire state if Event == CxH::event_t
      template<typename Event, typename ...V>
      void operator()(V &&...vals) {
        this->operator_case(
          std::integral_constant<
            bool,
            std::is_same<Event, typename Cx::event_t>::value
          >{},
          std::forward<V>(vals)...
        );
      }
    };
    
    template<template<typename> class EventPredicate,
             typename EventValues, typename CxH, typename ...CxT>
    struct completions_state<EventPredicate, EventValues,
                             completions<CxH,CxT...>>:
        // head base class
        completions_state_head<EventPredicate<typename CxH::event_t>::value,
                               EventValues, CxH>,
        // tail base class
        completions_state<EventPredicate, EventValues,
                          completions<CxT...>> {

      using completions_t = completions<CxH, CxT...>;
      
      using head_t = completions_state_head<
          /*event_enabled=*/EventPredicate<typename CxH::event_t>::value,
          EventValues, CxH
        >;
      using tail_t = completions_state<EventPredicate, EventValues,
                                       completions<CxT...>>;

      static constexpr bool empty = head_t::empty && tail_t::empty;
      
      completions_state(completions<CxH,CxT...> &&cxs):
        head_t(cxs.head_moved()),
        tail_t(cxs.tail_moved()) {
      }

      head_t& head() { return static_cast<head_t&>(*this); }
      head_t const& head() const { return *this; }
      
      tail_t& tail() { return static_cast<tail_t&>(*this); }
      tail_t const& tail() const { return *this; }
      
      template<typename Event, typename ...V>
      void operator()(V &&...vals) {
        // fire the head element
        head_t::template operator()<Event>(vals...);
        // recurse to fire remaining elements
        tail_t::template operator()<Event>(std::move(vals)...);
      }
    };
  }

  //////////////////////////////////////////////////////////////////////
  // Packing a completions_state of rpc_cx's

  namespace detail {
    template<typename EventValues, typename Event, typename Fn>
    struct packing_skippable_dumb<
        detail::completions_state_head<
          /*event_enabled=*/true, EventValues, rpc_cx<Event,Fn>
        >
      > {
      using type = detail::completions_state_head<true, EventValues, rpc_cx<Event,Fn>>;

      template<typename Ub, bool skippable>
      static auto ubound(Ub ub, type const &s, std::integral_constant<bool,skippable>) ->
        decltype(packing<Fn>::ubound(ub, s.state_.fn_, std::false_type())) {
        return packing<Fn>::ubound(ub, s.state_.fn_, std::false_type());
      }
      
      template<bool skippable>
      static void pack(parcel_writer &w, type const &s, std::integral_constant<bool,skippable>) {
        packing<Fn>::pack(w, s.state_.fn_, std::false_type());
      }

      using unpacked_t = detail::completions_state_head<true, EventValues, rpc_cx<Event,unpacked_of_t<Fn>>>;
      
      static void skip(parcel_reader &r) {
        packing<Fn>::skip(r);
      }
      
      template<bool skippable>
      static void unpack(parcel_reader &r, void *into, std::integral_constant<bool,skippable>) {
        return packing<Fn>::unpack(r, into, std::false_type());
      }
    };
  }

  template<typename EventValues, typename Event, typename Fn>
  struct packing<
      detail::completions_state_head<
        /*event_enabled=*/true, EventValues, rpc_cx<Event,Fn>
      >
    >: detail::packing_skippable_smart<
      detail::completions_state_head<
        /*event_enabled=*/true, EventValues, rpc_cx<Event,Fn>
      >
    > {
  };

  template<typename EventValues, typename Cx>
  struct packing<
      detail::completions_state_head</*event_enabled=*/false, EventValues, Cx>
    >:
    packing_empty<
      detail::completions_state_head</*event_enabled=*/false, EventValues, Cx>
    > {
  };
  
  template<template<typename> class EventPredicate,
           typename EventValues>
  struct packing<
      detail::completions_state<EventPredicate, EventValues, completions<>>
    >:
    packing_empty<
      detail::completions_state<EventPredicate, EventValues, completions<>>
    > {
  };

  namespace detail {
    template<template<typename> class EventPredicate,
             typename EventValues, typename CxH, typename ...CxT>
    struct packing_skippable_dumb<
        detail::completions_state<EventPredicate, EventValues, completions<CxH,CxT...>>
      > {
      using type = detail::completions_state<EventPredicate, EventValues, completions<CxH,CxT...>>;

      template<typename Ub, bool skippable>
      static auto ubound(Ub ub, type const &cxs, std::integral_constant<bool,skippable>)
        -> decltype(packing<typename type::tail_t>::ubound(
          packing<typename type::head_t>::ubound(
            ub, cxs.head(), std::false_type()
          ),
          cxs.tail(), std::false_type()
        )) {
        return packing<typename type::tail_t>::ubound(
          packing<typename type::head_t>::ubound(
            ub, cxs.head(), std::false_type()
          ),
          cxs.tail(), std::false_type()
        );
      }

      template<bool skippable>
      static void pack(parcel_writer &w, type const &cxs, std::integral_constant<bool,skippable>) {
        packing<typename type::head_t>::pack(w, cxs.head(), std::false_type());
        packing<typename type::tail_t>::pack(w, cxs.tail(), std::false_type());
      }

      using unpacked_t = detail::completions_state<
          EventPredicate, EventValues,
          completions<unpacked_of_t<CxH>, unpacked_of_t<CxT>...>
        >;
      
      static void skip(parcel_reader &r) {
        packing<typename type::head_t>::skip(r);
        packing<typename type::tail_t>::skip(r);
      }

      template<bool skippable>
      static void unpack(parcel_reader &r, void *into, std::integral_constant<bool,skippable>) {
        // DANGER: we never actually call any completions_state constructors, only
        // the completions_state_head constructors in our inheritance chain.
        packing<typename type::head_t>::unpack(r, (typename type::head_t*)(type*)into, std::false_type());
        packing<typename type::tail_t>::unpack(r, (typename type::tail_t*)(type*)into, std::false_type());
      }
    };
  }

  template<template<typename> class EventPredicate,
           typename EventValues, typename CxH, typename ...CxT>
  struct packing<
      detail::completions_state<EventPredicate, EventValues, completions<CxH,CxT...>>
    >:
    detail::packing_skippable_smart<
      detail::completions_state<EventPredicate, EventValues, completions<CxH,CxT...>>
    > {
  };
  
  //////////////////////////////////////////////////////////////////////
  // detail::completions_returner: Manage return type for completions<...>
  // object. Construct one of these instances against a
  // detail::completions_state&. Call operator() to get the return value.

  namespace detail {
    template<template<typename> class EventPredicate,
             typename EventValues, typename Cxs>
    struct completions_returner;

    template<template<typename> class EventPredicate,
             typename EventValues>
    struct completions_returner<EventPredicate, EventValues, completions<>> {
      using return_t = void;

      completions_returner(
          completions_state<EventPredicate, EventValues, completions<>>&
        ) {
      }
      
      void operator()() const {/*return void*/}
    };
    
    template<template<typename> class EventPredicate,
             typename EventValues, typename Cxs,
             typename TailReturn>
    struct completions_returner_head;
    
    template<template<typename> class EventPredicate,
             typename EventValues, typename CxH_event, progress_level level, typename ...CxT,
             typename ...TailReturn_tuplees>
    struct completions_returner_head<
        EventPredicate, EventValues,
        completions<future_cx<CxH_event,level>, CxT...>,
        std::tuple<TailReturn_tuplees...>
      > {
      
      using return_t = std::tuple<
          future_from_tuple_t<
            // kind for promise-built future
            detail::future_kind_shref<detail::future_header_ops_result>,
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
            std::make_tuple(s.head().state_.pro_.get_future()),
            tail()
          )
        } {
      }
    };

    template<template<typename> class EventPredicate,
             typename EventValues, typename CxH_event, progress_level level, typename ...CxT,
             typename TailReturn_not_tuple>
    struct completions_returner_head<
        EventPredicate, EventValues,
        completions<future_cx<CxH_event,level>, CxT...>,
        TailReturn_not_tuple
      > {
      
      using return_t = std::tuple<
          future_from_tuple_t<
            // kind for promise-built future
            detail::future_kind_shref<detail::future_header_ops_result>,
            typename EventValues::template tuple_t<CxH_event>
          >,
          TailReturn_not_tuple
        >;
      
      return_t ans_;
      return_t&& operator()() { return std::move(ans_); }
      
      template<typename CxState, typename Tail>
      completions_returner_head(CxState &s, Tail &&tail):
        ans_{
          std::make_tuple(
            s.head().state_.pro_.get_future(),
            tail()
          )
        } {
      }
    };

    template<template<typename> class EventPredicate,
             typename EventValues, typename CxH_event, progress_level level, typename ...CxT>
    struct completions_returner_head<
        EventPredicate, EventValues,
        completions<future_cx<CxH_event,level>, CxT...>,
        void
      > {
      
      using return_t = future_from_tuple_t<
          // kind for promise-built future
          detail::future_kind_shref<detail::future_header_ops_result>,
          typename EventValues::template tuple_t<CxH_event>
        >;
      
      return_t ans_;
      return_t&& operator()() { return std::move(ans_); }
      
      template<typename CxState, typename Tail>
      completions_returner_head(CxState &s, Tail&&):
        ans_{
          s.head().state_.pro_.get_future()
        } {
      }
    };
    
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

      completions_returner(
          completions_state<
              EventPredicate, EventValues, completions<CxH,CxT...>
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

