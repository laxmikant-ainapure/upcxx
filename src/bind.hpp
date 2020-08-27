#ifndef _ddfd9ec8_a1f2_48b9_8797_fc579cedfb18
#define _ddfd9ec8_a1f2_48b9_8797_fc579cedfb18

#include <upcxx/future.hpp>
#include <upcxx/global_fnptr.hpp>
#include <upcxx/serialization.hpp>
#include <upcxx/utility.hpp>

#include <tuple>
#include <type_traits>
#include <utility>

namespace upcxx {
  //////////////////////////////////////////////////////////////////////////////
  // binding<T>: Specialization for how to bind a T argument within a
  // call to `upcxx::bind`.
  
  template<typename T>
  struct binding/*{
    // these must satisfy the type equality:
    //   deserialized_type_t<binding<T>::on_wire_type>
    //    ==
    //   binding<binding<T>::off_wire_type>::on_wire_type
    typedef on_wire_type;
    typedef off_wire_type;
    
    // the stripped type is usually the decayed type, except for types like
    // dist_object& which aren't decayed when bound.
    typedef stripped_type;

    // does off_wire return a future (false) or immediately ready value (true)
    static constexpr bool immediate;
    
    // on_wire: Compute the value to be serialized on the wire.
    static on_wire_type on_wire(T);
    
    // off_wire: Given a rvalue-reference to deserialized wire value,
    // produce the off-wire value or future of it.
    static off_wire_type off_wire(on_wire_type);
    // ** OR **
    static future<off_wire_type> off_wire(on_wire_type&);
  }*/;

  template<typename T>
  struct binding_trivial {
    using stripped_type = T;
    using on_wire_type = T;
    using off_wire_type = deserialized_type_t<T>;
    using off_wire_future_type = future1<detail::future_kind_result, off_wire_type>;
    static constexpr bool immediate = true;
    
    template<typename T1>
    static T1&& on_wire(T1 &&x) {
      return static_cast<T1&&>(x);
    }
    template<typename T1>
    static T1&& off_wire(T1 &&x) {
      return static_cast<T1&&>(x);
    }
    template<typename T1>
    static off_wire_future_type off_wire_future(T1 &&x) {
      return detail::make_fast_future(static_cast<T1&&>(x));
    }
  };

  // binding defaults to trivial
  template<typename T>
  struct binding: binding_trivial<T> {};

  // binding implicitly drops const and refs
  template<typename T>
  struct binding<T&>: binding<T> {
    // now that references are Serializable, we can store a reference
    // in a binding, and the referent gets serialized
    using stripped_type = const T&;
    using on_wire_type = const T&;
  };
  template<typename T>
  struct binding<T&&>: binding<T> {};
  template<typename T>
  struct binding<const T>: binding<T> {};
  template<typename T>
  struct binding<volatile T>: binding<T> {};

  // binding does not drop reference to function
  template<typename R, typename ...A>
  struct binding<R(&)(A...)>: binding_trivial<R(&)(A...)> {};
  
  /*////////////////////////////////////////////////////////////////////////////
  bound_function: Packable callable wrapping an internal callable and _all_
  bound arguments expected by callable (partial binds of just leading arugments
  is no longer supported). The "on-wire" type of each thing is stored in this
  object. Calling the object invokes "off-wire" translation to produce futures,
  and when those are all ready, then the callable is applied to the bound
  arguments. A future of the internal callable's return value is returned. If
  all of the callable and the bound arguments are trivially-binding, then the
  futures are all elided and invoking this object with immediate arguments is
  just like invoking its internal callable against leading bound arguments, no
  future returned by this level.

  Currently, bound_function only supports being called as an rvalue (only
  `operator()() &&` is defined). Semantically, this makes it a one-shot deal,
  thus allowing it to move the inner callable and arguments into the call
  operation for max performance. This is all our use-cases need, since rpc's
  only invoke their callable once and `upcxx::bind` isn't yet spec'd for the
  user to expecxt anything else. The other call types ought to be supported,
  but I've deferred that as it seemed to be breaking compilation with overload
  resolution issues.
  */////////////////////////////////////////////////////////////////////////////
  
  namespace detail {
    template<typename Tup>
    struct binding_all_immediate;
    template<>
    struct binding_all_immediate<std::tuple<>>: std::true_type {};
    template<typename T, typename ...Ts>
    struct binding_all_immediate<std::tuple<T,Ts...>> {
      static constexpr bool value = binding<T>::immediate && binding_all_immediate<std::tuple<Ts...>>::value;
    };
    
    template<
      typename Fn, typename BndTup/*std::tuple<B...>*/,

      typename BndIxs = detail::make_index_sequence<std::tuple_size<BndTup>::value>,
      
      // whether all of Fn and B... immediately available off-wire?
      bool all_immediate = binding<Fn>::immediate
                        && detail::binding_all_immediate<BndTup>::value
      >
    struct bound_function_base;
    
    template<typename Fn, typename ...B, int ...bi>
    struct bound_function_base<
        Fn, std::tuple<B...>, detail::index_sequence<bi...>,
        /*all_immediate=*/true
      > {
      
      typename binding<Fn>::on_wire_type fn_;
      std::tuple<typename binding<B>::on_wire_type...> b_;
      
      typename std::result_of<
          typename binding<Fn>::off_wire_type&&(
            typename binding<B>::off_wire_type&&...
          )
        >::type
      operator()() && {
        return binding<Fn>::off_wire(std::move(fn_)).operator()(
            binding<B>::off_wire(std::get<bi>(std::move(b_)))...
          );
      }
      
      // TODO: operator()() &
      // TODO: operator()() const&
    };
    
    template<typename Fn_off_wire, typename ...B_off_wire>
    struct bound_function_applicator {
      template<typename Fn1, typename ...B1>
      typename std::result_of<Fn_off_wire(B_off_wire...)>::type
      operator()(Fn1 &&fn, B1 &&...b) const {
        return static_cast<Fn1&&>(fn)(static_cast<B1&&>(b)...);
      }
    };
    
    template<typename Fn, typename ...B, int ...bi>
    struct bound_function_base<
        Fn, std::tuple<B...>, detail::index_sequence<bi...>,
        /*all_immediate=*/false
      > {
      
      typename binding<Fn>::on_wire_type fn_;
      std::tuple<typename binding<B>::on_wire_type...> b_;
      
      auto operator()() &&
        UPCXX_RETURN_DECLTYPE(
          std::declval<future1<
            detail::future_kind_when_all<
              typename binding<Fn>::off_wire_future_type,
              typename binding<B>::off_wire_future_type...
            >,
            typename binding<Fn>::off_wire_type,
            typename binding<B>::off_wire_type...
          >>()
          .then_lazy(bound_function_applicator<
              typename binding<Fn>::off_wire_type,
              typename binding<B>::off_wire_type...
            >()
          )
        ) {
        return detail::when_all_fast(
            binding<Fn>::off_wire_future(std::move(fn_)),
            binding<B>::off_wire_future(std::get<bi>(std::move(b_)))...
          ).then_lazy(bound_function_applicator<
              typename binding<Fn>::off_wire_type,
              typename binding<B>::off_wire_type...
            >()
          );
      }
      // TODO: operator()() &
      // TODO: operator()() const&
    };
  }
  
  template<typename Fn, typename ...B>
  struct bound_function:
      detail::bound_function_base<Fn, std::tuple<B...>> {
    
    using base_type = detail::bound_function_base<Fn, std::tuple<B...>>;
    
    bound_function(
        typename binding<Fn>::on_wire_type &&fn,
        std::tuple<typename binding<B>::on_wire_type...> &&b
      ):
      base_type{std::move(fn), std::move(b)} {
    }

    // inherits operator()
  };

  template<typename Fn, typename ...B>
  using bound_function_of = bound_function<
      typename binding<Fn>::stripped_type,
      typename binding<B>::stripped_type...
    >;

  /*////////////////////////////////////////////////////////////////////////////
  deserialized_bound_function: The result of deserializing a bound_function.

  A bound_function<Fn, B...> deserializes as a
  deserialized_bound_function<Fn, B...> -- the type arguments match.

  Differences between bound_function and deserialized_bound_function:
  1) bound_function stores the arguments pre-serialization, while
     deserialized_bound_function stores them post-serialization.
  2) deserialized_bound_function can avoid moves by deserializing the
     components directly into raw internal storage rather than reading
     them into temporary storage and then moving them internally.
  */////////////////////////////////////////////////////////////////////////////

  namespace detail {
    template<
      typename Fn, typename BndTup/*std::tuple<B...>*/,

      typename BndIxs = detail::make_index_sequence<std::tuple_size<BndTup>::value>,

      // whether all of Fn and B... immediately available off-wire?
      bool all_immediate = binding<Fn>::immediate
                        && detail::binding_all_immediate<BndTup>::value
      >
    struct deserialized_bound_function_base;

    template<typename Fn, typename ...B, int ...bi>
    struct deserialized_bound_function_base<
        Fn, std::tuple<B...>, detail::index_sequence<bi...>,
        /*all_immediate=*/true
      > {

      template<typename T>
      using on_wire_type = typename binding<T>::on_wire_type;
      template<typename T>
      using stored_type = deserialized_type_t<on_wire_type<T>>;

      // raw storage for Fn and B...
      detail::raw_storage<stored_type<Fn>> raw_fn_;
      std::tuple<raw_storage<stored_type<B>>...> raw_b_;

      // deserialize the components directly into the internal raw storage
      template<typename Reader>
      deserialized_bound_function_base(Reader &r) {
        r.template read_into<on_wire_type<Fn>,
                             /*AssertSerializable=*/false>(raw_fn_.raw());
        int dummy[sizeof...(bi)] = {
          (r.template read_into<on_wire_type<B>>(std::get<bi>(raw_b_).raw()),
           0)...
        };
      }

      deserialized_bound_function_base(const deserialized_bound_function_base&) = delete;

      // because we use raw storage for the components, we have to
      // manually do all the work in the move constructor and destructor
      deserialized_bound_function_base(deserialized_bound_function_base &&other) {
        new(raw_fn_.raw()) stored_type<Fn>(std::move(other.raw_fn_.value()));
        int dummy[sizeof...(bi)] = {
          (new(std::get<bi>(raw_b_).raw()) stored_type<B>(
             std::move(std::get<bi>(other.raw_b_).value())
           ), 0)...
        };
      }

      ~deserialized_bound_function_base() {
        raw_fn_.destruct();
        int dummy[sizeof...(bi)] = {
          (std::get<bi>(raw_b_).destruct(), 0)...
        };
      }

      typename std::result_of<
          typename binding<Fn>::off_wire_type&&(
            typename binding<B>::off_wire_type&&...
          )
        >::type
      operator()() && {
        return binding<Fn>::off_wire(std::move(raw_fn_.value())).operator()(
            binding<B>::off_wire(std::move(std::get<bi>(raw_b_).value()))...
          );
      }

      // TODO: operator()() &
      // TODO: operator()() const&
    };

    template<typename Fn, typename ...B, int ...bi>
    struct deserialized_bound_function_base<
        Fn, std::tuple<B...>, detail::index_sequence<bi...>,
        /*all_immediate=*/false
      > {

      template<typename T>
      using on_wire_type = typename binding<T>::on_wire_type;
      template<typename T>
      using stored_type = deserialized_type_t<on_wire_type<T>>;

      // see the comments in the prior specialization of
      // deserialization_bound_function_base for how all this works
      detail::raw_storage<stored_type<Fn>> raw_fn_;
      std::tuple<raw_storage<stored_type<B>>...> raw_b_;

      template<typename Reader>
      deserialized_bound_function_base(Reader &r) {
        r.template read_into<on_wire_type<Fn>,
                             /*AssertSerializable=*/false>(raw_fn_.raw());
        int dummy[sizeof...(bi)] = {
          (r.template read_into<on_wire_type<B>>(std::get<bi>(raw_b_).raw()),
           0)...
        };
      }

      deserialized_bound_function_base(const deserialized_bound_function_base&) = delete;

      deserialized_bound_function_base(deserialized_bound_function_base &&other) {
        new(raw_fn_.raw()) stored_type<Fn>(std::move(other.raw_fn_.value()));
        int dummy[sizeof...(bi)] = {
          (new(std::get<bi>(raw_b_).raw()) stored_type<B>(
             std::move(std::get<bi>(other.raw_b_).value())
           ), 0)...
        };
      }

      ~deserialized_bound_function_base() {
        raw_fn_.destruct();
        int dummy[sizeof...(bi)] = {
          (std::get<bi>(raw_b_).destruct(), 0)...
        };
      }

      auto operator()() &&
        UPCXX_RETURN_DECLTYPE(
          std::declval<future1<
            detail::future_kind_when_all<
              typename binding<Fn>::off_wire_future_type,
              typename binding<B>::off_wire_future_type...
            >,
            typename binding<Fn>::off_wire_type,
            typename binding<B>::off_wire_type...
          >>()
          .then_lazy(bound_function_applicator<
              typename binding<Fn>::off_wire_type,
              typename binding<B>::off_wire_type...
            >()
          )
        ) {
        return detail::when_all_fast(
            binding<Fn>::off_wire_future(std::move(raw_fn_.value())),
            binding<B>::off_wire_future(std::move(std::get<bi>(raw_b_).value()))...
          ).then_lazy(bound_function_applicator<
              typename binding<Fn>::off_wire_type,
              typename binding<B>::off_wire_type...
            >()
          );
      }
      // TODO: operator()() &
      // TODO: operator()() const&
    };
  }

  template<typename Fn, typename ...B>
  struct deserialized_bound_function:
      detail::deserialized_bound_function_base<Fn, std::tuple<B...>> {

    using base_type =
      detail::deserialized_bound_function_base<Fn, std::tuple<B...>>;

    template<typename Reader>
    deserialized_bound_function(Reader &r) : base_type(r) {}
    deserialized_bound_function(const deserialized_bound_function&) = delete;
    deserialized_bound_function(deserialized_bound_function&&) = default;

    // inherits operator()
  };

  /////////////////////////////////////////////////////////////////////////////
  
  // make `bound_function` serializable
  template<typename Fn, typename ...B>
  struct serialization<bound_function<Fn,B...>> {
    static constexpr bool is_serializable =
      /* ignore serializability of Fn to allow for non-TriviallyCopyable lambdas */
      serialization_traits<std::tuple<typename binding<B>::on_wire_type...>>::is_serializable;

    template<typename Ub>
    static auto ubound(Ub ub, const bound_function<Fn,B...> &fn)
      UPCXX_RETURN_DECLTYPE(
        ub.template cat_ubound_of<typename binding<Fn>::on_wire_type>(fn.fn_)
          .template cat_ubound_of<std::tuple<typename binding<B>::on_wire_type...>>(fn.b_)
      ) {
      return ub.template cat_ubound_of<typename binding<Fn>::on_wire_type>(fn.fn_)
               .template cat_ubound_of<std::tuple<typename binding<B>::on_wire_type...>>(fn.b_);
    }
    
    template<typename Writer>
    static void serialize(Writer &w, const bound_function<Fn,B...> &fn) {
      w.template write<typename binding<Fn>::on_wire_type, /*AssertSerializable=*/false>(fn.fn_);
      serialize_args(w, fn, detail::make_index_sequence<sizeof...(B)>());
    }

    template<typename Writer, int ...bi>
    static void serialize_args(Writer &w, const bound_function<Fn,B...> &fn,
                               detail::index_sequence<bi...>) {
      // Since we deserialize each argument individually, we have to
      // serialize them individually as well. Otherwise, behavior
      // would depend on the representation of a std::tuple in the
      // TriviallySerializable case.
      int dummy[sizeof...(B)] = {
        (w.template write<typename binding<B>::on_wire_type>(
           std::get<bi>(fn.b_)
         ), 0)...
      };
    }

    using deserialized_type = deserialized_bound_function<Fn, B...>;
    
    static constexpr bool references_buffer = 
      serialization_traits<typename binding<Fn>::on_wire_type>::references_buffer ||
      serialization_traits<std::tuple<typename binding<B>::on_wire_type...>>::references_buffer;
    
    static constexpr bool skip_is_fast =
      serialization_traits<typename binding<Fn>::on_wire_type>::skip_is_fast &&
      serialization_traits<std::tuple<typename binding<B>::on_wire_type...>>::skip_is_fast;
    
    template<typename Reader>
    static void skip(Reader &r) {
      r.template skip<typename binding<Fn>::on_wire_type>();
      r.template skip<std::tuple<typename binding<B>::on_wire_type...>>();
    }

    template<typename Reader>
    static deserialized_type* deserialize(Reader &r, void *spot) {
      // deserialized_bound_function handles all its own deserialization
      return new(spot) deserialized_type(r);
    }
  };
}


////////////////////////////////////////////////////////////////////////////////
// upcxx::bind: Similar to std::bind but doesn't support placeholders. Most
// importantly, these can be packed. The `binding` typeclass is used for
// producing the on-wire and off-wire representations. If the wrapped callable
// and all bound arguments have trivial binding traits, then the returned
// callable has a return type equal to that of the wrapped callable. Otherwise,
// the returned callable will have a future return type.

namespace upcxx {
  namespace detail {
    // `upcxx::bind` defers to `upcxx::detail::bind` class which specializes
    // on `binding<Fn>::type` to detect the case of
    // `bind(bind(f, a...), b...)` and flattens it to `bind(f,a...,b...)`.
    // This optimization results in fewer chained futures for non-trivial
    // bindings.

    template<typename Fn, typename FnStripped, typename ...B>
    struct bind1;
    
    template<typename Fn, typename ...B>
    struct bind: bind1<Fn, typename binding<Fn>::stripped_type, B...> {};

    // general case
    template<typename Fn, typename FnStripped, typename ...B>
    struct bind1 {
      using return_type = bound_function_of<
          typename detail::globalize_fnptr_return<FnStripped>::type,
          B...
        >;

      bound_function_of<
          typename detail::globalize_fnptr_return<FnStripped>::type,
          B...
        >
      operator()(Fn fn, B ...b) const {
        using globalized_fn_t = typename detail::globalize_fnptr_return<FnStripped>::type;

        globalized_fn_t gfn = detail::globalize_fnptr(std::forward<Fn>(fn));
        
        return bound_function_of<globalized_fn_t, B...>{
          binding<globalized_fn_t>::on_wire(static_cast<globalized_fn_t>(gfn)),
          std::tuple<typename binding<B>::on_wire_type...>{
            binding<B>::on_wire(std::forward<B>(b))...
          }
        };
      }
    };

    // nested bind(bind(...),...) case.
    template<typename Bf, typename Fn0, typename ...B0, typename ...B1>
    struct bind<Bf, bound_function<Fn0, B0...>, B1...> {
      using return_type = bound_function_of<Fn0, B0..., B1...>;
      
      bound_function_of<Fn0, B0..., B1...>
      operator()(Bf bf, B1 ...b1) const {
        return bound_function_of<Fn0, B0..., B1...>{
          std::forward<Bf>(bf).fn_,
          std::tuple_cat(
            std::forward<Bf>(bf).b_, 
            std::tuple<typename binding<B1>::on_wire_type...>{
              binding<B1>::on_wire(std::forward<B1>(b1))...
            }
          )
        };
      }
    };
  }
  
  template<typename Fn, typename ...B>
  typename detail::template bind<Fn&&, B&&...>::return_type
  bind(Fn &&fn, B &&...b) {
    return detail::bind<Fn&&, B&&...>()(
      std::forward<Fn>(fn), std::forward<B>(b)...
    );
  }
}

#endif
