#ifndef _1e7a65b7_b8d1_4def_98a3_76038c9431cf
#define _1e7a65b7_b8d1_4def_98a3_76038c9431cf

#include <upcxx/future/core.hpp>

namespace upcxx {
  //////////////////////////////////////////////////////////////////////
  // future1: The actual type users get (aliased as future<>).
  
  template<typename Kind, typename ...T>
  struct future1 {
    typedef Kind kind_type;
    typedef std::tuple<T...> results_type;
    typedef typename Kind::template with_types<T...> impl_type;
    
    impl_type impl_;
    
  public:
    future1() = default;
    ~future1() = default;
    
    future1(impl_type impl): impl_{std::move(impl)} {}
    template<typename impl_type1>
    future1(impl_type1 impl): impl_{std::move(impl)} {}
    
    future1(future1 const&) = default;
    template<typename Kind1>
    future1(future1<Kind1,T...> const &that): impl_{that.impl_} {}
    
    future1(future1&&) = default;
    template<typename Kind1>
    future1(future1<Kind1,T...> &&that): impl_{std::move(that.impl_)} {}
    
    future1& operator=(future1 const&) = default;
    template<typename Kind1>
    future1& operator=(future1<Kind1,T...> const &that) {
      this->impl_ = that.impl_;
      return *this;
    }
    
    future1& operator=(future1&&) = default;
    template<typename Kind1>
    future1& operator=(future1<Kind1,T...> &&that) {
      this->impl_ = std::move(that.impl_);
      return *this;
    }
    
    inline bool ready() const {
      return impl_.ready();
    }
    
    template<int i=0>
    typename std::tuple_element<i, results_type>::type result() const {
      return impl_.template result<i>();
    }
    
    results_type results() const {
      return results_type{impl_.template results_refs_getter()()};
    }
    
    auto results_moved()
      -> decltype(upcxx::tuple_rvalues(impl_.template results_refs_getter()())) {
      return upcxx::tuple_rvalues(impl_.template results_refs_getter()());
    }
    
    template<typename Fn>
    auto then(Fn &&fn)
      -> decltype(
        detail::future_then<future1<Kind,T...>, typename std::decay<Fn>::type>()(
          *this,
          std::forward<Fn>(fn)
        )
      ) {
      return detail::future_then<future1<Kind,T...>, typename std::decay<Fn>::type>()(
        *this,
        std::forward<Fn>(fn)
      );
    }
    
    template<typename Fn>
    auto then_pure(Fn &&pure_fn)
      -> decltype(
        detail::future_then_pure<future1<Kind,T...>, typename std::decay<Fn>::type>()(
          *this,
          std::forward<Fn>(pure_fn)
        )
      ) {
      return detail::future_then_pure<future1<Kind,T...>, typename std::decay<Fn>::type>()(
        *this,
        std::forward<Fn>(pure_fn)
      );
    }
  };
}
#endif
