#ifndef _ddfd9ec8_a1f2_48b9_8797_fc579cedfb18
#define _ddfd9ec8_a1f2_48b9_8797_fc579cedfb18

#include <upcxx/packing.hpp>
#include <upcxx/utility.hpp>

#include <tuple>
#include <type_traits>
#include <utility>

namespace upcxx {
  //////////////////////////////////////////////////////////////////////
  // bound_function: Packable callable wrapping an internal callable
  // with some bound arguments.
  
  template<typename Fn, typename ...Binds>
  struct bound_function {
    Fn fn_;
    std::tuple<Binds...> b_;
    
    template<typename Me, int ...bi, typename ...Arg>
    static auto _apply(
        Me &&me,
        upcxx::index_sequence<bi...> b_seq,
        Arg &&...a
      )
      -> decltype(
        me.fn_(
          std::get<bi>(
            const_cast<std::tuple<Binds...>&>(me.b_)
          )...,
          std::forward<Arg>(a)...
        )
      ) {
      return me.fn_(
        std::get<bi>(
          const_cast<std::tuple<Binds...>&>(me.b_)
        )...,
        std::forward<Arg>(a)...
      );
    }
    
    template<typename ...Arg>
    auto operator()(Arg &&...a) const
      -> decltype(
        _apply(*this,
          upcxx::make_index_sequence<sizeof...(Binds)>(),
          std::forward<Arg>(a)...
        )
      ) {
      return _apply(*this,
        upcxx::make_index_sequence<sizeof...(Binds)>(),
        std::forward<Arg>(a)...
      );
    }
    
    template<typename ...Arg>
    auto operator()(Arg &&...a)
      -> decltype(
        _apply(*this,
          upcxx::make_index_sequence<sizeof...(Binds)>(),
          std::forward<Arg>(a)...
        )
      ) {
      return _apply(*this,
        upcxx::make_index_sequence<sizeof...(Binds)>(),
        std::forward<Arg>(a)...
      );
    }
  };
  
  // make it packable
  template<typename Fn, typename ...Binds>
  struct packing<bound_function<Fn,Binds...>> {
    static void size_ubound(parcel_layout &ub, const bound_function<Fn,Binds...> &fn) {
      packing<Fn>::size_ubound(ub, fn.fn_);
      packing<std::tuple<Binds...>>::size_ubound(ub, fn.b_);
    }
    static void pack(parcel_writer &w, const bound_function<Fn,Binds...> &fn) {
      packing<Fn>::pack(w, fn.fn_);
      packing<std::tuple<Binds...>>::pack(w, fn.b_);
    }
    
    typedef bound_function<
        typename unpacking<Fn>::type,
        typename unpacking<Binds>::type...
      > unpacking_type;
    
    static auto unpack(parcel_reader &r)
      -> bound_function<
        decltype(unpacking<Fn>::unpack(r)),
        decltype(unpacking<Binds>::unpack(r))...
      > {
      auto fn = unpacking<Fn>::unpack(r);
      auto binds = unpacking<std::tuple<Binds...>>::unpack(r);
      return bound_function<
          decltype(unpacking<Fn>::unpack(r)),
          decltype(unpacking<Binds>::unpack(r))...
        > {
          std::move(fn),
          std::move(binds)
        };
    }
  };
}


////////////////////////////////////////////////////////////////////////
// upcxx::bind: Similar to std::bind, but can be packed.

namespace upcxx {
  template<typename Fn, typename ...Binds>
  bound_function<Fn,Binds...> bind(Fn fn, Binds ...binds) {
    return bound_function<Fn,Binds...>{
      std::move(fn),
      std::tuple<Binds...>{std::move(binds)...}
    };
  }
  
  template<typename Fn>
  Fn bind(Fn fn) {
    return std::move(fn);
  }
}


////////////////////////////////////////////////////////////////////////
// upcxx::bind_last: bind except the function is last.

namespace upcxx {
  namespace detail {
    template<typename ...Parm, int ...heads, int tail>
    auto bind_last(
        std::tuple<Parm...> parms,
        upcxx::index_sequence<heads...>,
        std::integral_constant<int,tail>
      )
      -> bound_function<
        typename std::tuple_element<tail, std::tuple<Parm...>>::type,
        typename std::tuple_element<heads, std::tuple<Parm...>>::type...
      > {
      return bound_function<
          typename std::tuple_element<tail, std::tuple<Parm...>>::type,
          typename std::tuple_element<heads, std::tuple<Parm...>>::type...
        > {
          std::move(std::get<tail>(parms)),
          std::tuple<
            typename std::tuple_element<heads, std::tuple<Parm...>>::type...
            > {
            std::move(std::get<heads>(parms))...
          }
        };
    }
  }
  
  template<typename ...Parm>
  auto bind_last(Parm ...parm)
    -> decltype(
      detail::bind_last(
        std::tuple<Parm...>{std::move(parm)...},
        upcxx::make_index_sequence<sizeof...(Parm)-1>(),
        std::integral_constant<int, sizeof...(Parm)-1>()
      )
    ) {
    return detail::bind_last(
      std::tuple<Parm...>{std::move(parm)...},
      upcxx::make_index_sequence<sizeof...(Parm)-1>(),
      std::integral_constant<int, sizeof...(Parm)-1>()
    );
  }
}
#endif
