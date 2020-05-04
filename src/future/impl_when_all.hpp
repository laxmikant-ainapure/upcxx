#ifndef _3e3a0cc8_bebc_49a7_9f94_53f023c2bd53
#define _3e3a0cc8_bebc_49a7_9f94_53f023c2bd53

#include <upcxx/future/core.hpp>
#include <upcxx/future/body_pure.hpp>
#include <upcxx/utility.hpp>

#include <initializer_list>

namespace upcxx {
  //////////////////////////////////////////////////////////////////////
  // future_is_trivially_ready: future_impl_when_all specialization
  
  template<typename ...Arg, typename ...T>
  struct future_is_trivially_ready<
      future1<detail::future_kind_when_all<Arg...>, T...>
    > {
    static constexpr bool value = detail::trait_forall<upcxx::future_is_trivially_ready, Arg...>::value;
  };
  
  namespace detail {
    ////////////////////////////////////////////////////////////////////
    // future_impl_when_all: Future implementation concatenating
    // results of multiple futures.
    
    template<typename ArgTuple, typename ...T>
    struct future_impl_when_all;
    
    template<typename ...FuArg, typename ...T>
    struct future_impl_when_all<std::tuple<FuArg...>, T...> {
      std::tuple<FuArg...> args_;
    
    private:
      template<typename Bool0, typename ...Bools>
      static bool all_(Bool0 x0, Bools ...xs) { return x0 & all_(xs...); }
      static bool all_() { return true; }
      
      template<int ...i>
      bool ready_(detail::index_sequence<i...>) const {
        return all_(std::get<i>(this->args_).impl_.ready()...);
      }
      
      template<int ...i>
      auto result_refs_or_vals_(detail::index_sequence<i...>) const&
        UPCXX_RETURN_DECLTYPE(std::tuple_cat(
            std::get<i>(this->args_).impl_.result_refs_or_vals()...
          )
        ) {
        return std::tuple_cat(
          std::get<i>(this->args_).impl_.result_refs_or_vals()...
        );
      }
      template<int ...i>
      auto result_refs_or_vals_(detail::index_sequence<i...>) &&
        UPCXX_RETURN_DECLTYPE(std::tuple_cat(
            std::get<i>(std::move(this->args_)).impl_.result_refs_or_vals()...
          )
        ) {
        return std::tuple_cat(
          std::get<i>(std::move(this->args_)).impl_.result_refs_or_vals()...
        );
      }
      
    public:
      template<typename ...FuArg1>
      future_impl_when_all(FuArg1 &&...args):
        args_(static_cast<FuArg1&&>(args)...) {
      }
      
      bool ready() const {
        return this->ready_(detail::make_index_sequence<sizeof...(FuArg)>());
      }
      
      auto result_refs_or_vals() const&
        UPCXX_RETURN_DECLTYPE(this->result_refs_or_vals_(detail::make_index_sequence<sizeof...(FuArg)>())) {
        return this->result_refs_or_vals_(detail::make_index_sequence<sizeof...(FuArg)>());
      }
      auto result_refs_or_vals() &&
        UPCXX_RETURN_DECLTYPE(std::move(*this).result_refs_or_vals_(detail::make_index_sequence<sizeof...(FuArg)>())) {
        return std::move(*this).result_refs_or_vals_(detail::make_index_sequence<sizeof...(FuArg)>());
      }
      
      typedef future_header_ops_general header_ops;
      
      future_header* steal_header() {
        future_header_dependent *hdr = new future_header_dependent;
        
        using body_type = future_body_pure<future1<future_kind_when_all<FuArg...>,T...>>;
        void *body_mem = body_type::operator new(sizeof(body_type));
        
        hdr->body_ = ::new(body_mem) body_type(body_mem, hdr, std::move(*this));
        
        if(hdr->status_ == future_header::status_active)
          hdr->entered_active();
        
        return hdr;
      }
    };
    
    
    //////////////////////////////////////////////////////////////////////
    // future_dependency: future_impl_when_all specialization
    
    template<int i, typename Arg>
    struct future_dependency_when_all_arg {
      future_dependency<Arg> dep_;

      template<typename Arg1>
      future_dependency_when_all_arg(
          future_header_dependent *suc_hdr,
          Arg1 &&arg
        ):
        dep_(suc_hdr, static_cast<Arg1&&>(arg)) {
      }
    };
    
    template<typename AllArg, typename IxSeq>
    struct future_dependency_when_all_base;
    
    template<typename ...Arg, typename ...T, int ...i>
    struct future_dependency_when_all_base<
        future1<future_kind_when_all<Arg...>, T...>,
        detail::index_sequence<i...>
      >:
      // variadically inherit from each future_dependency specialization
      private future_dependency_when_all_arg<i,Arg>... {
      
      typedef future_dependency_when_all_base<
          future1<future_kind_when_all<Arg...>, T...>,
          detail::index_sequence<i...>
        > this_t;

      template<typename FuArg1>
      future_dependency_when_all_base(
          future_header_dependent *suc_hdr,
          /*future1<future_kind_when_all<Arg...>, T...>*/FuArg1 &&all_args
        ):
        future_dependency_when_all_arg<i,Arg>(
          suc_hdr,
          std::get<i>(static_cast<FuArg1&&>(all_args).impl_.args_)
        )... {
      }
      
      void cleanup_early() {
        // run cleanup_early on each base class
        std::initializer_list<int>{(
          static_cast<future_dependency_when_all_arg<i,Arg>*>(this)->dep_.cleanup_early(),
          0
        )...};
      }
      
      void cleanup_ready() {
        // run cleanup_ready on each base class
        std::initializer_list<int>{(
          static_cast<future_dependency_when_all_arg<i,Arg>*>(this)->dep_.cleanup_ready(),
          0
        )...};
      }
      
      auto result_refs_or_vals() const&
        UPCXX_RETURN_DECLTYPE(
          std::tuple_cat(
            static_cast<future_dependency_when_all_arg<i,Arg> const&>(*this).dep_.result_refs_or_vals()...
          )
        ) {
        return std::tuple_cat(
          static_cast<future_dependency_when_all_arg<i,Arg> const&>(*this).dep_.result_refs_or_vals()...
        );
      }

      auto result_refs_or_vals() &&
        UPCXX_RETURN_DECLTYPE(
          std::tuple_cat(
            static_cast<future_dependency_when_all_arg<i,Arg>&&>(*this).dep_.result_refs_or_vals()...
          )
        ) {
        return std::tuple_cat(
          static_cast<future_dependency_when_all_arg<i,Arg>&&>(*this).dep_.result_refs_or_vals()...
        );
      }
    };
    
    template<typename ...Arg, typename ...T>
    struct future_dependency<
        future1<future_kind_when_all<Arg...>, T...>
      >:
      future_dependency_when_all_base<
        future1<future_kind_when_all<Arg...>, T...>,
        detail::make_index_sequence<sizeof...(Arg)>
      > {

      template<typename Arg1>
      future_dependency(
          future_header_dependent *suc_hdr,
          /*future1<future_kind_when_all<Arg...>, T...>*/Arg1 &&arg
        ):
        future_dependency_when_all_base<
            future1<future_kind_when_all<Arg...>, T...>,
            detail::make_index_sequence<sizeof...(Arg)>
          >(suc_hdr, static_cast<Arg1&&>(arg)) {
      }
      
      future_header* cleanup_ready_get_header() {
        future_header *hdr = &(new future_header_result<T...>(
            /*not_ready=*/false,
            /*values=*/static_cast<future_dependency&&>(*this).result_refs_or_vals()
          ))->base_header;
        this->cleanup_ready();
        return hdr;
      }
    };
  }
}
#endif
