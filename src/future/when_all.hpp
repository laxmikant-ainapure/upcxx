#ifndef _eb1a60f5_4086_4689_a513_8486eacfd815
#define _eb1a60f5_4086_4689_a513_8486eacfd815

#include <upcxx/future/core.hpp>
#include <upcxx/future/impl_when_all.hpp>

namespace upcxx {
  //////////////////////////////////////////////////////////////////////
  // when_all()
  
  namespace detail {
    // compute return type of when_all
    template<typename ...Arg>
    using when_all_return_t = 
      future_from_tuple_t<
        future_kind_when_all<Arg...>,
        decltype(std::tuple_cat(
          std::declval<typename Arg::results_type>()...
        ))
      >;
  }
  
  template<typename ...Arg>
  detail::when_all_return_t<Arg...> when_all(Arg ...args) {
    return typename detail::when_all_return_t<Arg...>::impl_type{
      std::move(args)...
    };
  }
}
#endif
