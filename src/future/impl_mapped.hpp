#ifndef _c554ca08_f811_43b3_8a89_b20c52b59a1c
#define _c554ca08_f811_43b3_8a89_b20c52b59a1c

#include <upcxx/future/core.hpp>
#include <upcxx/future/apply.hpp>
#include <upcxx/future/body_pure.hpp>

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
        arg_{std::move(arg)},
        fn_{std::move(fn)} {
      }
      
      bool ready() const {
        return this->arg_.impl_.ready();
      }
      
      struct results_refs_function {
        typedef future_apply_return_t<Fn(FuArg)> fn_return_t;
        typedef decltype(std::declval<fn_return_t>().impl_.results_refs_getter()) fn_return_getter_t;
        
        fn_return_t result_;
        fn_return_getter_t getter_;
        
        results_refs_function(fn_return_t result):
          result_{std::move(result)},
          getter_{result_.impl_.results_refs_getter()} {
        }
        
        results_refs_function(const results_refs_function &that):
          result_{that.result_},
          getter_{this->result_.impl_.results_refs_getter()} {
        }
        results_refs_function& operator=(const results_refs_function &that) {
          this->result_ = that.result_;
          this->getter_ = this->result_.impl_.results_refs_getter();
          return *this;
        }
        
        results_refs_function(results_refs_function &&that):
          result_{std::move(that.result_)},
          getter_{this->result_.impl_.results_refs_getter()} {
        }
        results_refs_function& operator=(results_refs_function &&that) {
          this->result_ = std::move(that.result_);
          this->getter_ = this->result_.impl_.results_refs_getter();
          return *this;
        }
        
        auto operator()() const
          -> decltype(getter_()) {
          return getter_();
        }
      };
      
      results_refs_function results_refs_getter() const {
        return results_refs_function{
          future_apply<Fn(FuArg)>::apply(
            this->fn_,
            this->arg_.impl_.results_refs_getter()()
          )
        };
      }
      
      template<int i=0>
      typename std::tuple_element<i,std::tuple<T...>>::type value() const {
        return std::get<i>(this->results_refs_getter());
      }
    
    public:
      typedef future_header_ops_general header_ops;
      
      future_header* steal_header() {
        future_header_dependent *hdr = new future_header_dependent;
        
        typedef future_body_pure<future1<future_kind_mapped<FuArg,Fn>,T...>> body_type;
        void *body_mem = operator new(sizeof(body_type));
        
        body_type *body = new(body_mem) body_type{
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
      
      typedef typename future_impl_mapped<FuArg,Fn,T...>::results_refs_function results_refs_function;
      
      results_refs_function results_refs_getter() const {
        return results_refs_function{
          future_apply<Fn(FuArg)>()(
            this->fn_,
            this->dep_.results_refs_getter()()
          )
        };
      }
      
      future_header* cleanup_ready_get_header() {
        future_header *hdr = new future_header_result<T...>{
          /*anon_deps_n=*/0,
          this->results_refs_getter()()
        };
        
        dep_.cleanup_ready();
        
        return hdr;
      }
    };
  }
}
#endif
