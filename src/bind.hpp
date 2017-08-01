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
    
    // off_wire: Given the deserialized wire value, produce a future
    // to the off-wire type.
    static future<off_wire_type> off_wire(U);
  }*/;
  
  template<typename T>
  struct binding {
    using on_wire_type = T;
    using off_wire_type = T;
    
    static T on_wire(T x) {
      return std::move(x);
    }
    
    static auto off_wire(T x)
      -> decltype(make_future(std::move(x))) {
      return make_future(std::move(x));
    }
    
    // This is the trivial binding of type T that uses T itself for
    // the on-wire type and a trivially ready future<T> as off-wire.
    static constexpr bool is_trivial = true;
  };
  
  template<typename T, typename trivial=std::false_type>
  struct binding_is_trivial {
    static constexpr bool value = trivial::value;
  };
  template<typename T>
  struct binding_is_trivial<T, std::integral_constant<bool, binding<T>::is_trivial>> {
    static constexpr bool value = binding<T>::is_trivial;
  };
  
  // binding implicitly decays
  template<typename T>
  struct binding<T&>: binding<T> {};
  template<typename T>
  struct binding<T&&>: binding<T> {};
  template<typename T>
  struct binding<T const>: binding<T> {};
  template<typename T>
  struct binding<T volatile>: binding<T> {};
  
  
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
  
  template<typename Fn, typename BndTup, bool all_trivial>
  struct bound_function_base;
  
  template<typename Fn, typename ...B>
  struct bound_function_base<
      Fn, std::tuple<B...>, /*all_trivial=*/true
    > {
    
    Fn fn_;
    std::tuple<B...> b_;
    
    template<typename Me, int ...bi, typename ...Arg>
    static auto apply_(
        Me &&me,
        upcxx::index_sequence<bi...> b_seq,
        Arg &&...a
      )
      -> decltype(
        me.fn_(
          std::get<bi>(
            const_cast<std::tuple<B...>&>(me.b_)
          )...,
          std::forward<Arg>(a)...
        )
      ) {
      return me.fn_(
        std::get<bi>(
          const_cast<std::tuple<B...>&>(me.b_)
        )...,
        std::forward<Arg>(a)...
      );
    }
  };
  
  template<typename Fn, typename ...B>
  struct bound_function_base<
      Fn, std::tuple<B...>, /*all_trivial=*/false
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
        -> decltype(apply_(
          upcxx::make_index_sequence<sizeof...(Arg)>{},
          fn, b...
        )) {
        return apply_(
          upcxx::make_index_sequence<sizeof...(Arg)>{},
          fn, b...
        );
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
          binding<Fn>::off_wire(me.fn_),
          binding<B>::off_wire(
            std::get<bi>(
              const_cast<std::tuple<typename binding<B>::on_wire_type...>&>(me.b_)
            )
          )...
        ) >> std::declval<applicator<Arg&&...>>()
      ) {
      return upcxx::when_all(
          binding<Fn>::off_wire(me.fn_),
          binding<B>::off_wire(
            std::get<bi>(
              const_cast<std::tuple<typename binding<B>::on_wire_type...>&>(me.b_)
            )
          )...
        ) >> applicator<Arg&&...>{
          std::tuple<Arg&&...>{std::forward<Arg>(a)...}
        };
    }
  };
  
  template<typename Fn, typename ...B>
  struct bound_function: bound_function_base<
      Fn, std::tuple<B...>,
      /*all_trivial=*/binding_is_trivial<Fn>::value && upcxx::trait_forall<binding_is_trivial, B...>::value
    > {
    
    using base_type = bound_function_base<
        Fn, std::tuple<B...>,
        /*all_trivial=*/binding_is_trivial<Fn>::value && upcxx::trait_forall<binding_is_trivial, B...>::value
      >;
    
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
// representations.

namespace upcxx {
  template<typename Fn, typename ...B>
  bound_function<Fn&&, B&&...> bind(Fn &&fn, B &&...b) {
    return bound_function<Fn&&, B&&...>{
      binding<Fn&&>::on_wire(std::forward<Fn>(fn)),
      std::tuple<typename binding<B&&>::on_wire_type...>{
        binding<B&&>::on_wire(std::forward<B>(b))...
      }
    };
  }
  
  template<typename Fn>
  Fn&& bind(Fn &&fn) {
    return std::forward<Fn>(fn);
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
      -> bound_function<
        typename binding<typename std::tuple_element<tail, std::tuple<P...>>::type>::on_wire_type,
        typename binding<typename std::tuple_element<heads, std::tuple<P...>>::type>::on_wire_type...
      > {
      return bound_function<
          typename binding<typename std::tuple_element<tail, std::tuple<P...>>::type>::on_wire_type,
          typename binding<typename std::tuple_element<heads, std::tuple<P...>>::type>::on_wire_type...
        > {
          typename binding<typename std::tuple_element<tail, std::tuple<P...>>::type>::on_wire(
            std::move(std::get<tail>(parms))
          ),
          std::tuple<
              typename binding<typename std::tuple_element<heads, std::tuple<P...>>::type>::on_wire_type...
            > {
            typename binding<typename std::tuple_element<heads, std::tuple<P...>>::type>::on_wire(
              std::move(std::get<heads>(parms))
            )...
          }
        };
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
  Fn&& bind_last(Fn &&fn) {
    return std::forward<Fn>(fn);
  }
}
#endif
