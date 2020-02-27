#ifndef _c1e666db_a184_42a1_8492_49b8a1d9259d
#define _c1e666db_a184_42a1_8492_49b8a1d9259d

#include <upcxx/future/core.hpp>
#include <upcxx/utility.hpp>

namespace upcxx {
  //////////////////////////////////////////////////////////////////////
  // future_is_trivially_ready: future_impl_result specialization
  
  template<typename ...T>
  struct future_is_trivially_ready<
      future1<detail::future_kind_result, T...>
    > {
    static constexpr bool value = true;
  };
  
  namespace detail {
    ////////////////////////////////////////////////////////////////////
    // future_impl_result: implementation of a trivially ready value
    
    template<typename ...T>
    struct future_impl_result {
      std::tuple<T...> results_;
      
    public:
      future_impl_result(T ...values):
        results_{std::forward<T>(values)...} {
      }
      
      bool ready() const { return true; }
      
      detail::tuple_refs_return_t<std::tuple<T...> const&>
      result_refs_or_vals() const& {
        return detail::tuple_refs(results_);
      }
      detail::tuple_refs_return_t<std::tuple<T...>&&>
      result_refs_or_vals() && {
        return detail::tuple_refs(std::move(results_));
      }
      
      typedef future_header_ops_result_ready header_ops;
      
      future_header* steal_header() {
        return &(new future_header_result<T...>(0, std::move(results_)))->base_header;
      }
    };
    
    template<>
    struct future_impl_result<> {
      bool ready() const { return true; }
      
      std::tuple<> result_refs_or_vals() const { return std::tuple<>(); }
      
      typedef future_header_ops_result_ready header_ops;
      
      future_header* steal_header() {
        return &future_header_result<>::the_always;
      }
    };
    
    
    ////////////////////////////////////////////////////////////////////
    // future_dependency: future_impl_result specialization
    
    template<typename ...T>
    struct future_dependency<
        future1<future_kind_result, T...>
      > {
      std::tuple<T...> results_;
      
      future_dependency(
          future_header_dependent *suc_hdr,
          future1<future_kind_result, T...> arg
        ):
        results_{std::move(arg.impl_.results_)} {
      }
      
      void cleanup_early() {}
      
      detail::tuple_refs_return_t<std::tuple<T...> const&>
      result_refs_or_vals() const& {
        return detail::tuple_refs(results_);
      }
      detail::tuple_refs_return_t<std::tuple<T...>&&>
      result_refs_or_vals() && {
        return detail::tuple_refs(std::move(results_));
      }
      
      void cleanup_ready() {}
      
      future_header* cleanup_ready_get_header() {
        return &(new future_header_result<T...>(
            /*not_ready=*/false,
            /*values=*/std::move(results_)
          ))->base_header;
      }
    };
    
    template<>
    struct future_dependency<
        future1<future_kind_result>
      > {
      future_dependency(
          future_header_dependent *suc_hdr,
          future1<future_kind_result> arg
        ) {
      }
      
      void cleanup_early() {}
      
      std::tuple<> result_refs_or_vals() const {
        return std::tuple<>();
      }
      
      void cleanup_ready() {}
      
      future_header* cleanup_ready_get_header() {
        return &(new future_header_result<>(
            /*not_ready=*/false,
            /*value=*/std::tuple<>()
          ))->base_header;
      }
    };
  }
}  
#endif
