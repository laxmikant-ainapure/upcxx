#ifndef _e1661a2a_f7f6_44f7_97dc_4f3e6f4fd018
#define _e1661a2a_f7f6_44f7_97dc_4f3e6f4fd018

#include <upcxx/future/core.hpp>
#include <upcxx/future/make_future.hpp>

namespace upcxx {
namespace detail {
  //////////////////////////////////////////////////////////////////////
  
  // dispatch on return type of function call
  template<typename Return>
  struct apply_tupled_as_future_impl;
  
  // return = void
  template<>
  struct apply_tupled_as_future_impl<void> {
    typedef decltype(upcxx::make_future<>()) return_type;
    
    template<typename Fn, typename ArgTup>
    return_type operator()(Fn &&fn, ArgTup &&argtup) {
      upcxx::apply_tupled(std::forward<Fn>(fn), std::forward<ArgTup>(argtup));
      return upcxx::make_future<>();
    }
  };
  
  // return not a future, return != void
  template<typename Return>
  struct apply_tupled_as_future_impl {
    typedef decltype(upcxx::make_future<Return>(std::declval<Return>())) return_type;
    
    template<typename Fn, typename ArgTup>
    return_type operator()(Fn &&fn, ArgTup &&argtup) {
      return upcxx::make_future<Return>(
        upcxx::apply_tupled(std::forward<Fn>(fn), std::forward<ArgTup>(argtup))
      );
    }
  };
  
  // return is future
  template<typename Kind, typename ...T>
  struct apply_tupled_as_future_impl<future1<Kind,T...>> {
    typedef future1<Kind,T...> return_type;
    
    template<typename Fn, typename ArgTup>
    return_type operator()(Fn &&fn, ArgTup &&argtup) {
      return upcxx::apply_tupled(std::forward<Fn>(fn), std::forward<ArgTup>(argtup));
    }
  };
  
  //////////////////////////////////////////////////////////////////////
  
  // augment apply_tupled_as_future with decayed tupled type
  template<typename Fn, typename ArgTup,
           typename ArgTupDecayed = typename std::decay<ArgTup>::type>
  struct apply_tupled_as_future_help;
  
  template<typename Fn, typename ArgTup, typename ...T>
  struct apply_tupled_as_future_help<Fn, ArgTup, std::tuple<T...>>:
    apply_tupled_as_future_impl<
      typename std::result_of<Fn(T...)>::type
    > {
  };

  //////////////////////////////////////////////////////////////////////
  // from: future/core.hpp
  
  template<typename Fn, typename ArgTup>
  struct apply_tupled_as_future:
    apply_tupled_as_future_help<Fn, ArgTup> {
  };
  
  template<typename Fn, typename Kind, typename ...T>
  struct apply_futured_as_future<Fn(future1<Kind,T...>)> {
    using tupled_impl = apply_tupled_as_future<
        Fn,
        std::tuple<typename upcxx::add_lref_if_nonref<T>::type...>
      >;
    
    using return_type = typename tupled_impl::return_type;
    
    template<typename Fn1>
    return_type operator()(Fn1 &&fn, future1<Kind,T...> const &arg) {
      return tupled_impl()(
        std::forward<Fn1>(fn),
        arg.impl_.result_lrefs_getter()()
      );
    }
    
    template<typename Fn1>
    return_type operator()(Fn1 &&fn, future_dependency<future1<Kind,T...>> const &arg) {
      return tupled_impl()(
        std::forward<Fn1>(fn),
        arg.result_lrefs_getter()()
      );
    }
  };
}}

namespace upcxx {
  template<typename Fn, typename ArgTup>
  auto apply_tupled_as_future(Fn &&fn, ArgTup &&argtup)
    -> decltype(
      detail::apply_tupled_as_future<Fn,ArgTup>()(
        std::forward<Fn>(fn), std::forward<ArgTup>(argtup)
      )
    ) {
    return detail::apply_tupled_as_future<Fn,ArgTup>()(
      std::forward<Fn>(fn), std::forward<ArgTup>(argtup)
    );
  }
}
#endif
