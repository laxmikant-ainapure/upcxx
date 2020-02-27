#ifndef _c554ca08_f811_43b3_8a89_b20c52b59a1c
#define _c554ca08_f811_43b3_8a89_b20c52b59a1c

#include <upcxx/future/core.hpp>
#include <upcxx/future/apply.hpp>
#include <upcxx/future/body_pure.hpp>
#include <upcxx/utility.hpp>

namespace upcxx {
  //////////////////////////////////////////////////////////////////////
  // future_is_trivially_ready: future_impl_mapped specialization
  
  template<typename Arg, typename Fn, typename ...T>
  struct future_is_trivially_ready<
      future1<detail::future_kind_mapped<Arg,Fn>,T...>
    > {
    static constexpr bool value = future_is_trivially_ready<Arg>::value;
  };
  
  namespace detail {
    ////////////////////////////////////////////////////////////////////
    // future_impl_mapped: Pure-function bound to an underlying future.
    
    template<typename FuArg, typename Fn, typename ...T>
    struct future_impl_mapped {
      FuArg arg_;
      Fn fn_;
    
    public:
      future_impl_mapped(FuArg arg, Fn fn):
        arg_(std::move(arg)),
        fn_(std::move(fn)) {
      }
      
      bool ready() const {
        return this->arg_.impl_.ready();
      }
      
      using fn_return_t = apply_futured_as_future_return_t<Fn,FuArg>;
      static_assert(future_is_trivially_ready<fn_return_t>::value, "Mapped futures must return trivially ready futures.");
      
      std::tuple<T...> result_refs_or_vals() const& {
        return std::tuple<T...>(
          apply_futured_as_future<Fn const&, FuArg const&>()(this->fn_, this->arg_)
            .impl_.result_refs_or_vals()
        );
      }

      std::tuple<T...> result_refs_or_vals() && {
        return std::tuple<T...>(
          apply_futured_as_future<Fn&&, FuArg&&>()(std::move(this->fn_), std::move(this->arg_))
            .impl_.result_refs_or_vals()
        );
      }
      
    public:
      typedef future_header_ops_general header_ops;
      
      future_header* steal_header() {
        future_header_dependent *hdr = new future_header_dependent;
        
        typedef future_body_pure<future1<future_kind_mapped<FuArg,Fn>,T...>> body_type;
        void *body_mem = body_type::operator new(sizeof(body_type));
        
        body_type *body = ::new(body_mem) body_type{
          body_mem, hdr, std::move(*this)
        };
        hdr->body_ = body;
        
        if(hdr->status_ == future_header::status_active)
          hdr->entered_active();
        
        return hdr;
      }
    };
    
    
    //////////////////////////////////////////////////////////////////////
    // future_dependency: future_impl_mapped specialization
    
    template<typename FuArg, typename Fn, typename ...T>
    struct future_dependency<
        future1<future_kind_mapped<FuArg,Fn>, T...>
      > {
      future_dependency<FuArg> dep_;
      Fn fn_;
      
    public:
      future_dependency(
          future_header_dependent *suc_hdr,
          future1<future_kind_mapped<FuArg,Fn>, T...> arg
        ):
        dep_{suc_hdr, std::move(arg.impl_.arg_)},
        fn_{std::move(arg.impl_.fn_)} {
      }
      
      void cleanup_early() { dep_.cleanup_early(); }
      void cleanup_ready() { dep_.cleanup_ready(); }

      std::tuple<T...> result_refs_or_vals() const& {
        return std::tuple<T...>(
          apply_futured_as_future<Fn const&, FuArg const&>()(this->fn_, this->dep_)
            .impl_.result_refs_or_vals()
        );
      }

      std::tuple<T...> result_refs_or_vals() && {
        return std::tuple<T...>(
          apply_futured_as_future<Fn&&, FuArg&&>()(std::move(this->fn_), std::move(this->dep_))
            .impl_.result_refs_or_vals()
        );
      }
      
      future_header* cleanup_ready_get_header() {
        future_header *hdr = &(new future_header_result<T...>(
            /*not_ready=*/false,
            /*values=*/std::move(*this).result_refs_or_vals()
          ))->base_header;
        
        dep_.cleanup_ready();
        
        return hdr;
      }
    };
  }
}
#endif
