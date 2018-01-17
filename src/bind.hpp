#ifndef _ddfd9ec8_a1f2_48b9_8797_fc579cedfb18
#define _ddfd9ec8_a1f2_48b9_8797_fc579cedfb18

#include <upcxx/future.hpp>
#include <upcxx/packing.hpp>
#include <upcxx/utility.hpp>

#include <tuple>
#include <type_traits>
#include <utility>

namespace upcxx {
  //////////////////////////////////////////////////////////////////////
  // binding<T>: Specialization for how to bind a T argument within a
  // call to `upcxx::bind`.
  
  template<typename T>
  struct binding/*{
    typedef on_wire_type;
    typedef off_wire_type;
    
    // on_wire: Compute the value to be serialized on the wire.
    static on_wire_type on_wire(T);
    
    // off_wire: Given a lvalue-reference to deserialized wire value,
    // produce a future of the off-wire type as a value or future-of-value.
    static off_wire_type off_wire(on_wire_type&);
    // -- or --
    static future<off_wire_type> off_wire(on_wire_type&);
  }*/;
  
  template<typename T>
  struct binding_trivial {
    using on_wire_type = T;
    using off_wire_type = T;
    
    static T on_wire(T x) { return std::move(x); }
    static T off_wire(T &&x) { return std::move(x); }
    static T& off_wire(T &x) { return x; }
  };

  // binding defaults to trivial
  template<typename T>
  struct binding: binding_trivial<T> {};

  // binding of reference-to-function is handled on the wire as a
  // pointer-to-function.
  template<typename Ret, typename ...Arg>
  struct binding<Ret(&)(Arg...)> {
    typedef Ret(*on_wire_type)(Arg...);
    typedef Ret(&off_wire_type)(Arg...);
    
    static on_wire_type on_wire(off_wire_type x) { return &x; }
    static off_wire_type off_wire(on_wire_type x) { return *x; }
  };
  
  // binding implicitly decays (except references-to-functions handled above).
  template<typename T>
  struct binding<T&>: binding<T> {};
  template<typename T>
  struct binding<T&&>: binding<T&> {};
  template<typename T>
  struct binding<T const>: binding<T> {};
  template<typename T>
  struct binding<T volatile>: binding<T> {};
  
  //////////////////////////////////////////////////////////////////////
  // binding_is_immediate: immediate bindings are those that don't return
  // futures from off_wire()
  
  template<typename T,
           typename Offed = decltype(
             binding<T>::off_wire(std::declval<typename binding<T>::on_wire_type&>())
           )>
  struct binding_is_immediate: std::true_type {};

  template<typename T, typename Kind, typename ...U>
  struct binding_is_immediate<T, /*Offed=*/future1<Kind,U...>>: std::false_type {};
  
  //////////////////////////////////////////////////////////////////////
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
      
      // whether all of Fn and B... immediately available off-wire?
      bool all_immediate = binding_is_immediate<Fn>::value
                        && upcxx::trait_forall_tupled<binding_is_immediate, BndTup>::value
      >
    struct bound_function_base;
    
    template<typename Fn, typename ...B>
    struct bound_function_base<
        Fn, std::tuple<B...>, /*all_immediate=*/true
      > {
      
      typename binding<Fn>::on_wire_type fn_;
      std::tuple<typename binding<B>::on_wire_type...> b_;

      template<typename T>
      static T& as_lref(T &&rref) { return rref; }
      
      template<typename Me, int ...bi, typename ...Arg>
      static auto apply_(
          Me &&me,
          upcxx::index_sequence<bi...> b_seq,
          Arg &&...a
        )
        -> decltype(
          binding<Fn>::off_wire(me.fn_)(
            std::declval<typename binding<B>::off_wire_type&>()...,
            std::forward<Arg>(a)...
          )
        ) {
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
    
    template<typename Fn, typename ...B>
    struct bound_function_base<
        Fn, std::tuple<B...>, /*all_immediate=*/false
      > {
      
      typename binding<Fn>::on_wire_type fn_;
      std::tuple<typename binding<B>::on_wire_type...> b_;
      
      template<typename ...Arg>
      struct applicator {
        std::tuple<Arg...> a;
        
        template<int ...ai>
        auto apply_(
            upcxx::index_sequence<ai...>,
            typename binding<Fn>::off_wire_type &fn,
            typename binding<B>::off_wire_type &...b
          )
          -> decltype(fn(b..., std::get<ai>(a)...)) {
          return fn(b..., std::get<ai>(a)...);
        }
        
        auto operator()(
            typename binding<Fn>::off_wire_type &fn,
            typename binding<B>::off_wire_type &...b
          )
          -> decltype(
            apply_(upcxx::make_index_sequence<sizeof...(Arg)>{}, fn, b...)
          ) {
          return apply_(upcxx::make_index_sequence<sizeof...(Arg)>{}, fn, b...);
        };
      };
      
      template<typename Me, int ...bi, typename ...Arg>
      static auto apply_(
          Me &&me,
          upcxx::index_sequence<bi...> b_seq,
          Arg &&...a
        )
        -> decltype(
          upcxx::when_all(
            to_future(
              binding<Fn>::off_wire(std::declval<typename binding<Fn>::on_wire_type&&>())
            ),
            to_future(
              binding<B>::off_wire(std::declval<typename binding<B>::on_wire_type&&>())
            )...
          ) >> std::declval<applicator<Arg&&...>>()
        ) {
        return upcxx::when_all(
            to_future(
              binding<Fn>::off_wire(std::move(me.fn_))
            ),
            to_future(
              binding<B>::off_wire(
                static_cast<typename binding<B>::on_wire_type&&>(
                  const_cast<typename binding<B>::on_wire_type&>(std::get<bi>(me.b_))
                )
              )
            )...
          ) >> applicator<Arg&&...>{
            std::tuple<Arg&&...>{std::forward<Arg>(a)...}
          };
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
      -> decltype(
        base_type::apply_(*this,
          upcxx::make_index_sequence<sizeof...(B)>(),
          std::forward<Arg>(a)...
        )
      ) {
      return base_type::apply_(*this,
        upcxx::make_index_sequence<sizeof...(B)>(),
        std::forward<Arg>(a)...
      );
    }
    
    template<typename ...Arg>
    auto operator()(Arg &&...a)
      -> decltype(
        base_type::apply_(*this,
          upcxx::make_index_sequence<sizeof...(B)>(),
          std::forward<Arg>(a)...
        )
      ) {
      return base_type::apply_(*this,
        upcxx::make_index_sequence<sizeof...(B)>(),
        std::forward<Arg>(a)...
      );
    } 
  };
  
  // make `bound_function` packable
  template<typename Fn, typename ...B>
  struct packing<bound_function<Fn,B...>> {
    static void size_ubound(parcel_layout &ub, const bound_function<Fn,B...> &fn) {
      packing<typename binding<Fn>::on_wire_type>::size_ubound(ub, fn.fn_);
      packing<std::tuple<typename binding<B>::on_wire_type...>>::size_ubound(ub, fn.b_);
    }
    
    static void pack(parcel_writer &w, const bound_function<Fn,B...> &fn) {
      packing<typename binding<Fn>::on_wire_type>::pack(w, fn.fn_);
      packing<std::tuple<typename binding<B>::on_wire_type...>>::pack(w, fn.b_);
    }
    
    static bound_function<Fn,B...> unpack(parcel_reader &r) {
      auto fn = packing<typename binding<Fn>::on_wire_type>::unpack(r);
      auto b = packing<std::tuple<typename binding<B>::on_wire_type...>>::unpack(r);
      
      return bound_function<Fn,B...>{std::move(fn), std::move(b)};
    }
  };
}


////////////////////////////////////////////////////////////////////////
// upcxx::bind: Similar to std::bind, but can be packed and the `binding`
// typeclass is used for producing the on-wire and off-wire
// representations. If the wrapped callable and all bound arguments have
// trivial binding traits, then the returned callable has a return
// type equal to that of the wrapped callable. Otherwise, the returned
// callable will have a future return type.

namespace upcxx {
  namespace detail {
    // `upcxx::bind` defers to `upcxx::detail::bind` class which specializes
    // on `std::decay<Fn>::type` to detect the case of
    // `bind(bind(f, a...), b...)` and flattens it to `bind(f,a...,b...)`.
    // This optimization results in fewer chained futures for non-trivial
    // bindings.
    
    // general case
    template<typename FnDecayed>
    struct bind {
      template<typename Fn, typename ...B>
      bound_function<Fn&&, B&&...> operator()(Fn &&fn, B &&...b) const {
        return bound_function<Fn&&, B&&...>{
          binding<Fn&&>::on_wire(std::forward<Fn>(fn)),
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
      bound_function<Fn0, B0..., B1&&...> operator()(Bf &&bf, B1 &&...b1) const {
        return bound_function<Fn0, B0..., B1&&...>{
          std::move(bf.fn_),
          std::tuple_cat(
            std::move(bf.b_), 
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
      detail::bind<typename std::decay<Fn>::type>()(
        std::forward<Fn>(fn), std::forward<B>(b)...
      )
    ) {
    return detail::bind<typename std::decay<Fn>::type>()(
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
        upcxx::index_sequence<heads...>,
        std::integral_constant<int,tail>
      )
      -> decltype(
        detail::bind<typename std::decay<typename std::tuple_element<tail, std::tuple<P...>>::type>::type>()(
          std::move(std::get<tail>(parms)),
          std::move(std::get<heads>(parms))...
        )
      ) {
      return detail::bind<typename std::decay<typename std::tuple_element<tail, std::tuple<P...>>::type>::type>()(
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
        upcxx::make_index_sequence<sizeof...(P)-1>(),
        std::integral_constant<int, sizeof...(P)-1>()
      )
    ) {
    return detail::bind_last(
      std::tuple<P&&...>{std::forward<P>(parm)...},
      upcxx::make_index_sequence<sizeof...(P)-1>(),
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
