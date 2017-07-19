#ifndef _e1661a2a_f7f6_44f7_97dc_4f3e6f4fd018
#define _e1661a2a_f7f6_44f7_97dc_4f3e6f4fd018

#include <upcxx/future/core.hpp>
#include <upcxx/future/make_future.hpp>

namespace upcxx {
namespace detail {
  //////////////////////////////////////////////////////////////////////
  // get the returned future type of a lambda(future) call
  
  template<typename Fn, typename Arg, typename Res>
  struct future_apply1;
  
  template<typename Fn, typename Kind, typename ...Arg>
  struct future_apply1<Fn, future1<Kind,Arg...>, void> {
    typedef decltype(upcxx::make_future<>()) return_type;
    
    template<typename Fn1, typename ArgTup>
    return_type operator()(Fn1 &&lam, ArgTup &&argtup) {
      upcxx::apply_tupled(lam, std::forward<ArgTup>(argtup));
      return upcxx::make_future<>();
    }
  };
  template<typename Fn, typename Kind, typename ...Arg, typename ResT>
  struct future_apply1<Fn, future1<Kind,Arg...>, ResT> {
    typedef decltype(upcxx::make_future<ResT>(std::declval<ResT>())) return_type;
    
    template<typename Fn1, typename ArgTup>
    return_type operator()(Fn1 &&lam, ArgTup &&argtup) {
      return upcxx::make_future<ResT>(
        upcxx::apply_tupled(lam, std::forward<ArgTup>(argtup))
      );
    }
  };
  template<typename Fn, typename Kind, typename ...Arg, typename Kind1, typename ...ResT>
  struct future_apply1<
      Fn, future1<Kind,Arg...>,
      future1<Kind1,ResT...>
    > {
    typedef future1<Kind1,ResT...> return_type;
    
    template<typename Fn1, typename ArgTup>
    return_type operator()(Fn1 &&lam, ArgTup &&argtup) {
      return upcxx::apply_tupled(lam, std::forward<ArgTup>(argtup));
    }
  };
  
  template<typename Fn, typename ArgKind, typename ...ArgT>
  struct future_apply<Fn(future1<ArgKind,ArgT...>)>:
    future_apply1<
      Fn, future1<ArgKind,ArgT...>,
      typename std::result_of<Fn(ArgT&...)>::type
    > {
  };
}}
#endif
