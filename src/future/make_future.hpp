#ifndef _69b9dd2a_13a0_4c70_af49_33621d57d3bf
#define _69b9dd2a_13a0_4c70_af49_33621d57d3bf

#include <upcxx/future/impl_result.hpp>
#include <upcxx/future/impl_shref.hpp>

namespace upcxx {
  //////////////////////////////////////////////////////////////////////
  // make_future()
  
  namespace detail {
    template<bool trivial, typename ...T>
    struct make_future_;
    
    template<typename ...T>
    struct make_future_</*trivial=*/true, T...> {
      future1<future_kind_result, T...> operator()(T ...values) {
        return future_impl_result<T...>{std::move(values)...};
      }
    };
    template<typename ...T>
    struct make_future_</*trivial=*/false, T...> {
      future1<future_kind_shref<future_header_ops_result_ready>,T...>
      operator()(T ...values) {
        return future_impl_shref<future_header_ops_result_ready, T...>{
          new future_header_result<T...>{
            /*not_ready*/false,
            /*values*/std::tuple<T...>{std::move(values)...}
          }
        };
      }
    };
    
    template<typename ...T>
    struct make_future:
      make_future_<
        upcxx::trait_forall<std::is_trivially_copyable, T...>::value,
        T...
      > {
    };
  }
  
  template<typename ...T>
  auto make_future(T ...values)
    -> decltype(detail::make_future<T...>()(std::move(values)...)) {
    return detail::make_future<T...>()(std::move(values)...);
  }
  
  
  //////////////////////////////////////////////////////////////////////
  // to_future()
  
  template<typename T>
  auto to_future(T x)
    -> decltype(make_future(std::move(x))) {
    return make_future(std::move(x));
  }
  
  template<typename Kind, typename ...T>
  future1<Kind,T...> to_future(future1<Kind,T...> x) {
    return std::move(x);
  }
}
#endif
