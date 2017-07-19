#ifndef _3e3a0cc8_bebc_49a7_9f94_53f023c2bd53
#define _3e3a0cc8_bebc_49a7_9f94_53f023c2bd53

#include <upcxx/future/core.hpp>
#include <upcxx/future/body_pure.hpp>

namespace upcxx {
  //////////////////////////////////////////////////////////////////////
  // future_is_trivially_ready: future_impl_when_all specialization
  
  template<typename ...Arg, typename ...T>
  struct future_is_trivially_ready<
      future1<detail::future_kind_when_all<Arg...>, T...>
    > {
    static constexpr bool value = upcxx::trait_forall<future_is_trivially_ready, Arg...>::value;
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
      bool ready_(upcxx::index_sequence<i...>) const {
        return all_(std::get<i>(this->args_).impl_.ready()...);
      }
      
      template<int ...i>
      struct results_refs_function {
        typedef std::tuple<decltype(std::declval<FuArg>().impl_.results_refs_getter())...> getters_tuple;
        
        getters_tuple getters_;
        
        results_refs_function(std::tuple<FuArg...> const &args):
          getters_{std::get<i>(args).impl_.results_refs_getter()...} {
        }
        
        auto operator()() const
          -> decltype(std::tuple_cat(std::get<i>(getters_)()...)) {
          return std::tuple_cat(std::get<i>(getters_)()...);
        }
      };
      
      template<int ...i>
      results_refs_function<i...> results_refs_getter_(upcxx::index_sequence<i...>) const {
        return results_refs_function<i...>{this->args_};
      }
    
    public:
      future_impl_when_all(FuArg ...args):
        args_{std::move(args)...} {
      }
      
      bool ready() const {
        return this->ready_(upcxx::make_index_sequence<sizeof...(FuArg)>());
      }
      
      auto results_refs_getter() const
        -> decltype(this->results_refs_getter_(upcxx::make_index_sequence<sizeof...(FuArg)>())) {
        return this->results_refs_getter_(upcxx::make_index_sequence<sizeof...(FuArg)>());
      }
      
      template<int i=0>
      typename std::tuple_element<i,std::tuple<T...>>::type result() const {
        // implementing this to use a reference if the underlying future::result<j>()
        // (where j is the index within the correct sub-future) uses
        // a reference is too hard, just doing value copy.
        return std::get<i>(this->results_refs_getter());
      }
    
    public:
      typedef future_header_ops_general header_ops;
      
      future_header* steal_header() {
        future_header_dependent *hdr = new future_header_dependent;
        
        typedef future_body_pure<future1<future_kind_when_all<FuArg...>,T...>> body_type;
        void *body_mem = operator new(sizeof(body_type));
        
        hdr->body_ = new(body_mem) body_type{body_mem, hdr, std::move(*this)};
        
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
      
      future_dependency_when_all_arg(
          future_header_dependent *suc_hdr,
          Arg arg
        ):
        dep_{suc_hdr, std::move(arg)} {
      }
    };
    
    template<typename AllArg, typename IxSeq>
    struct future_dependency_when_all_base;
    
    template<typename ...Arg, typename ...T, int ...i>
    struct future_dependency_when_all_base<
        future1<future_kind_when_all<Arg...>, T...>,
        upcxx::index_sequence<i...>
      >:
      // variadically inherit from each future_dependency specialization
      private future_dependency_when_all_arg<i,Arg>... {
      
      typedef future_dependency_when_all_base<
          future1<future_kind_when_all<Arg...>, T...>,
          upcxx::index_sequence<i...>
        > this_t;
      
      future_dependency_when_all_base(
          future_header_dependent *suc_hdr,
          future1<future_kind_when_all<Arg...>, T...> all_args
        ):
        future_dependency_when_all_arg<i,Arg>{
          suc_hdr,
          std::move(std::get<i>(all_args.impl_.args_))
        }... {
      }
      
      template<typename ...U>
      static void force_each_(U ...x) {}
      
      void cleanup_early() {
        // run cleanup_early on each base class
        force_each_((
          static_cast<future_dependency_when_all_arg<i,Arg>*>(this)->dep_.cleanup_early(),
          0
        )...);
      }
      
      void cleanup_ready() {
        // run cleanup_ready on each base class
        force_each_((
          static_cast<future_dependency_when_all_arg<i,Arg>*>(this)->dep_.cleanup_ready(),
          0
        )...);
      }
      
      struct results_refs_function {
        std::tuple<decltype(std::declval<future_dependency<Arg>>().results_refs_getter())...> getters_;
        
        results_refs_function(this_t const *me):
          getters_{
            static_cast<future_dependency_when_all_arg<i,Arg> const*>(me)->dep_.results_refs_getter()...
          } {
        }
        
        auto operator()() const
          -> decltype(std::tuple_cat(std::get<i>(getters_)()...)) {
          return std::tuple_cat(std::get<i>(getters_)()...);
        }
      };
      
      results_refs_function results_refs_getter() const {
        return results_refs_function{this};
      }
    };
    
    template<typename ...Arg, typename ...T>
    struct future_dependency<
        future1<future_kind_when_all<Arg...>, T...>
      >:
      future_dependency_when_all_base<
        future1<future_kind_when_all<Arg...>, T...>,
        upcxx::make_index_sequence<sizeof...(Arg)>
      > {
      
      future_dependency(
          future_header_dependent *suc_hdr,
          future1<future_kind_when_all<Arg...>, T...> arg
        ):
        future_dependency_when_all_base<
            future1<future_kind_when_all<Arg...>, T...>,
            upcxx::make_index_sequence<sizeof...(Arg)>
          >{suc_hdr, std::move(arg)} {
      }
      
      future_header* cleanup_ready_get_header() {
        future_header *hdr = new future_header_result<T...>{
          /*anon_deps_n=*/0,
          this->results_refs_getter()()
        };
        this->cleanup_ready();
        return hdr;
      }
    };
  }
}
#endif
