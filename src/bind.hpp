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
    //   deserialized_type_of_t<binding<T>::on_wire_type>
    //    ==
    //   binding<binding<T>::off_wire_type>::on_wire_type
    typedef on_wire_type;
    typedef off_wire_type;
    
    // on_wire: Compute the value to be serialized on the wire.
    static on_wire_type on_wire(T);
    
    // off_wire: Given a lvalue-reference to deserialized wire value,
    // produce the off-wire value or future of it.
    static off_wire_type off_wire(on_wire_type);
    // ** OR **
    static future<off_wire_type> off_wire(on_wire_type&);
  }*/;

  template<typename T>
  struct binding_trivial {
    using stripped_type = T;
    using on_wire_type = T;
    using off_wire_type = deserialized_type_of_t<T>;

    template<typename T1>
    static T1&& on_wire(T1 &&x) {
      return static_cast<T1&&>(x);
    }
    template<typename T1>
    static T1&& off_wire(T1 &&x) {
      return static_cast<T1&&>(x);
    }
  };

  // binding defaults to trivial
  template<typename T>
  struct binding: binding_trivial<T> {};

  // binding implicitly drops const and refs
  template<typename T>
  struct binding<T&>: binding<T> {};
  template<typename T>
  struct binding<T&&>: binding<T> {};
  template<typename T>
  struct binding<const T>: binding<T> {};
  template<typename T>
  struct binding<volatile T>: binding<T> {};

  // binding does not drop reference to function
  template<typename R, typename ...A>
  struct binding<R(&)(A...)>: binding_trivial<R(&)(A...)> {};
  
  //////////////////////////////////////////////////////////////////////////////
  // binding_is_immediate: immediate bindings are those that don't return
  // futures from off_wire()
  
  template<typename T,
           typename Offed = decltype(
             binding<typename binding<T>::off_wire_type>
              ::off_wire(
                std::declval<deserialized_type_of_t<typename binding<T>::on_wire_type>&>()
              )
           )>
  struct binding_is_immediate: std::true_type {};

  template<typename T, typename Kind, typename ...U>
  struct binding_is_immediate<T, /*Offed=*/future1<Kind,U...>>: std::false_type {};
  
  //////////////////////////////////////////////////////////////////////////////
  // bound_function: Packable callable wrapping an internal callable
  // and bound leading arguments. The "on-wire" type of each thing is
  // stored in this object. Calling the object invokes "off-wire"
  // translation to produce futures, and when those are all ready, then
  // the callable is applied to the bound arguments followed by the
  // immediate arguments. A future of the internal callable's return
  // value is returned. If all of the callable and the bound arguments
  // are trivially-binding, then the futures are all elided and invoking
  // this object with immediate arguments is just like invoking its
  // internal callable against leading bound arguments + immediate
  // arguments, no future returned by this level.
  
  namespace detail {
    template<
      typename Fn, typename BndTup/*std::tuple<B...>*/,

      typename BndIxs = detail::make_index_sequence<std::tuple_size<BndTup>::value>,
      
      // whether all of Fn and B... immediately available off-wire?
      bool all_immediate = binding_is_immediate<Fn>::value
                        && detail::trait_forall_tupled<binding_is_immediate, BndTup>::value
      >
    struct bound_function_base;
    
    template<typename Fn, typename ...B, int ...bi>
    struct bound_function_base<
        Fn, std::tuple<B...>, detail::index_sequence<bi...>,
        /*all_immediate=*/true
      > {
      
      typename binding<Fn>::on_wire_type fn_;
      std::tuple<typename binding<B>::on_wire_type...> b_;

      template<typename T>
      static T& as_lref(T &&rref) { return rref; }
      
      template<typename Me, typename ...Arg>
      static auto apply_(Me &&me, Arg &&...a)
#ifndef __INTEL_COMPILER
        -> decltype(
          binding<Fn>::off_wire(me.fn_)(
            std::declval<typename binding<B>::off_wire_type&>()...,
            std::forward<Arg>(a)...
          )
        )
#endif
      {
        return binding<Fn>::off_wire(me.fn_)(
          as_lref(
            binding<B>::off_wire(
              const_cast<typename binding<B>::on_wire_type&>(
                std::get<bi>(me.b_)
              )
            )
          )...,
          std::forward<Arg>(a)...
        );
      }
    };

    template<typename Fn, typename BndTup, typename ArgTup,
             typename ArgIxs = detail::make_index_sequence<std::tuple_size<ArgTup>::value>>
    struct bound_function_applicator;

    template<typename Fn, typename ...B, typename ...Arg, int ...ai>
    struct bound_function_applicator<
        Fn, std::tuple<B...>, std::tuple<Arg...>, detail::index_sequence<ai...>
      > {

      std::tuple<Arg...> a;
      
      auto operator()(
          typename binding<Fn>::off_wire_type &fn,
          typename binding<B>::off_wire_type &...b
        ) ->
        decltype(fn(b..., std::get<ai>(a)...)) {
        return fn(b..., std::get<ai>(a)...);
      }
    };
    
    template<typename Fn, typename ...B, int ...bi>
    struct bound_function_base<
        Fn, std::tuple<B...>, detail::index_sequence<bi...>,
        /*all_immediate=*/false
      > {
      
      typename binding<Fn>::on_wire_type fn_;
      std::tuple<typename binding<B>::on_wire_type...> b_;
      
      template<typename Me, typename ...Arg>
      static auto apply_(Me &&me, Arg &&...a)
#ifndef __INTEL_COMPILER
        -> decltype(
          upcxx::when_all(
            upcxx::to_future(
              binding<Fn>::off_wire(
                std::declval<typename binding<Fn>::on_wire_type&&>()
              )
            ),
            upcxx::to_future(
              binding<B>::off_wire(
                std::declval<typename binding<B>::on_wire_type&&>()
              )
            )...
          ).then(
            std::declval<bound_function_applicator<
                Fn, std::tuple<B...>,
                std::tuple<typename binding<Arg&&>::stripped_type...>
              >>()
          )
        )
#endif
      {
        return upcxx::when_all(
            upcxx::to_future(
              binding<Fn>::off_wire(std::move(me.fn_))
            ),
            upcxx::to_future(
              binding<B>::off_wire(
                static_cast<typename binding<B>::on_wire_type&&>(
                  const_cast<typename binding<B>::on_wire_type&>(
                    std::get<bi>(me.b_)
                  )
                )
              )
            )...
          ).then(
            bound_function_applicator<
                Fn, std::tuple<B...>,
                std::tuple<typename binding<Arg&&>::stripped_type...>
              >{
              std::tuple<typename binding<Arg&&>::stripped_type...>{
                std::forward<Arg>(a)...
              }
            }
          );
      }
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
    
    template<typename ...Arg>
    auto operator()(Arg &&...a) const
#ifndef __INTEL_COMPILER
      -> decltype(base_type::apply_(*this, std::forward<Arg>(a)...))
#endif
    {
      return base_type::apply_(*this, std::forward<Arg>(a)...);
    }
    
    template<typename ...Arg>
    auto operator()(Arg &&...a)
#ifndef __INTEL_COMPILER
      -> decltype(base_type::apply_(*this, std::forward<Arg>(a)...))
#endif
    {
      return base_type::apply_(*this, std::forward<Arg>(a)... );
    }
  };

  template<typename Fn, typename ...B>
  using bound_function_of = bound_function<
      typename binding<Fn>::stripped_type,
      typename binding<B>::stripped_type...
    >;
  
  // make `bound_function` serializable
  template<typename Fn, typename ...B>
  struct serialization<bound_function<Fn,B...>> {
    static constexpr bool is_definitely_serializable =
      serialization_complete<typename binding<Fn>::on_wire_type>::is_definitely_serializable &&
      serialization_complete<std::tuple<typename binding<B>::on_wire_type...>>::is_definitely_serializable;

    template<typename Ub>
    static auto ubound(Ub ub, const bound_function<Fn,B...> &fn)
      -> decltype(
        ub.template cat_ubound_of<typename binding<Fn>::on_wire_type>(fn.fn_)
          .template cat_ubound_of<std::tuple<typename binding<B>::on_wire_type...>>(fn.b_)
      ) {
      return ub.template cat_ubound_of<typename binding<Fn>::on_wire_type>(fn.fn_)
               .template cat_ubound_of<std::tuple<typename binding<B>::on_wire_type...>>(fn.b_);
    }
    
    template<typename Writer>
    static void serialize(Writer &w, const bound_function<Fn,B...> &fn) {
      w.template push<typename binding<Fn>::on_wire_type>(fn.fn_);
      w.template push<std::tuple<typename binding<B>::on_wire_type...>>(fn.b_);
    }

    using deserialized_type = bound_function<
        typename binding<Fn>::off_wire_type,
        typename binding<B>::off_wire_type...
      >;
    
    static constexpr bool references_buffer = 
      serialization_complete<typename binding<Fn>::on_wire_type>::references_buffer &&
      serialization_complete<std::tuple<typename binding<B>::on_wire_type...>>::references_buffer;
    
    static constexpr bool skip_is_fast =
      serialization_complete<typename binding<Fn>::on_wire_type>::skip_is_fast &&
      serialization_complete<std::tuple<typename binding<B>::on_wire_type...>>::skip_is_fast;
    
    template<typename Reader>
    static void skip(Reader &r) {
      r.template skip<typename binding<Fn>::on_wire_type>();
      r.template skip<std::tuple<typename binding<B>::on_wire_type...>>();
    }

    template<typename Reader>
    static deserialized_type* deserialize(Reader &r, void *spot) {
      detail::raw_storage<deserialized_type_of_t<typename binding<Fn>::on_wire_type>> fn;
      r.template pop_into<typename binding<Fn>::on_wire_type>(&fn);

      detail::raw_storage<deserialized_type_of_t<std::tuple<typename binding<B>::on_wire_type...>>> b;
      r.template pop_into<std::tuple<typename binding<B>::on_wire_type...>>(&b);
      
      return ::new(spot) deserialized_type(
        fn.value_and_destruct(),
        b.value_and_destruct()
      );
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
    
    // general case
    template<typename FnStripped>
    struct bind {
      template<typename Fn, typename ...B>
      bound_function_of<
          decltype(detail::globalize_fnptr(std::declval<Fn&&>())),
          B&&...
        >
      operator()(Fn &&fn, B &&...b) const {
        decltype(detail::globalize_fnptr(std::declval<Fn&&>())) gfn
          = detail::globalize_fnptr(std::forward<Fn>(fn));

        return bound_function_of<decltype(gfn), B&&...>{
          binding<decltype(gfn)>::on_wire(static_cast<decltype(gfn)>(gfn)),
          std::tuple<typename binding<B&&>::on_wire_type...>{
            binding<B&&>::on_wire(std::forward<B>(b))...
          }
        };
      }
    };

    // nested bind(bind(...),...) case.
    template<typename Fn0, typename ...B0>
    struct bind<bound_function<Fn0, B0...>> {
      template<typename Bf, typename ...B1>
      bound_function_of<Fn0, B0..., B1&&...>
      operator()(Bf &&bf, B1 &&...b1) const {
        return bound_function_of<Fn0, B0..., B1&&...>{
          std::forward<Bf>(bf).fn_,
          std::tuple_cat(
            std::forward<Bf>(bf).b_, 
            std::tuple<typename binding<B1&&>::on_wire_type...>{
              binding<B1&&>::on_wire(std::forward<B1>(b1))...
            }
          )
        };
      }
    };
  }
  
  template<typename Fn, typename ...B>
  auto bind(Fn &&fn, B &&...b)
    -> decltype(
      detail::bind<typename binding<Fn&&>::stripped_type>()(
        std::forward<Fn>(fn), std::forward<B>(b)...
      )
    ) {
    return detail::bind<typename binding<Fn&&>::stripped_type>()(
      std::forward<Fn>(fn), std::forward<B>(b)...
    );
  }
}


////////////////////////////////////////////////////////////////////////
// upcxx::bind_last: bind except the function is last.

namespace upcxx {
  namespace detail {
    template<typename ...P, int ...heads, int tail>
    auto bind_last(
        std::tuple<P...> parms,
        detail::index_sequence<heads...>,
        std::integral_constant<int,tail>
      )
      -> decltype(
        detail::bind<typename binding<typename std::tuple_element<tail, std::tuple<P...>>::type>::type>()(
          std::move(std::get<tail>(parms)),
          std::move(std::get<heads>(parms))...
        )
      ) {
      return detail::bind<typename binding<typename std::tuple_element<tail, std::tuple<P...>>::type>::type>()(
        std::move(std::get<tail>(parms)),
        std::move(std::get<heads>(parms))...
      );
    }
  }
  
  template<typename ...P>
  auto bind_last(P &&...parm)
    -> decltype(
      detail::bind_last(
        std::tuple<P&&...>{std::forward<P>(parm)...},
        detail::make_index_sequence<sizeof...(P)-1>(),
        std::integral_constant<int, sizeof...(P)-1>()
      )
    ) {
    return detail::bind_last(
      std::tuple<P&&...>{std::forward<P>(parm)...},
      detail::make_index_sequence<sizeof...(P)-1>(),
      std::integral_constant<int, sizeof...(P)-1>()
    );
  }
  
  template<typename Fn>
  auto bind_last(Fn fn)
    -> decltype(bind(std::move(fn))) {
    return bind(std::move(fn));
  }
}
#endif
