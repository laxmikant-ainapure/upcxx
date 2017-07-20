#ifndef _1e7a65b7_b8d1_4def_98a3_76038c9431cf
#define _1e7a65b7_b8d1_4def_98a3_76038c9431cf

#include <upcxx/future/core.hpp>

namespace upcxx {
  //////////////////////////////////////////////////////////////////////
  /* FutureImpl concept:
   * 
   * struct FutureImpl {
   *   FutureImpl(FutureImpl const&);
   *   FutureImpl& operator=(FutureImpl const&);
   * 
   *   FutureImpl(FutureImpl&&);
   *   FutureImpl& operator=(FutureImpl&&);
   * 
   *   bool ready() const;
   * 
   *   // Return a no-argument callable which returns a tuple of lvalue-
   *   // references to results. The references are valid so long as
   *   // both this FutureImpl and the callable are alive. Tuple
   *   // componenets which are already reference type (& or &&) will
   *   // just have their type unaltered.
   *   LRefsGetter result_lrefs_getter() const;
   * 
   *   // Return result tuple where each component is either a value or
   *   // rvalue-reference depending on whether the reference
   *   // would be valid for as long as this future lives.
   *   // Therefor we guarantee that references will be valid as long
   *   // as this future lives. If any T are already & or && then they
   *   // are returned unaltered.
   *   tuple<(T or T&&)...> result_rvals();
   *   
   *   // Returns a future_header (with refcount included for consumer)
   *   // for this future. Leaves us in an undefined state (only safe
   *   // operations are (copy/move)-(construction/assignment), and
   *   // destruction).
   *   detail::future_header* steal_header();
   *   
   *   // One of the header operations classes for working with the
   *   // header produced by steal_header().
   *   typedef detail::future_header_ops_? header_ops;
   * };
   */
  
  //////////////////////////////////////////////////////////////////////
  // future1: The actual type users get (aliased as future<>).
  
  template<typename Kind, typename ...T>
  struct future1 {
    typedef Kind kind_type;
    typedef std::tuple<T...> results_type;
    typedef typename Kind::template with_types<T...> impl_type; // impl_type is a FutureImpl.
    
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
    
    bool ready() const {
      return impl_.ready();
    }
    
    template<int i=0>
    typename std::tuple_element<i, results_type>::type result() const {
      return typename std::tuple_element<i, results_type>::type{
        std::get<i>(impl_.template result_lrefs_getter()())
      };
    }
    
    results_type results() const {
      return results_type{impl_.template result_rvals()};
    }
    
    template<int i=0>
    auto result_moved()
      -> decltype(std::get<i>(impl_.template result_rvals())) {
      return std::get<i>(impl_.template result_rvals());
    }
    
    auto results_moved()
      -> decltype(impl_.template result_rvals()) {
      return impl_.template results_rvals();
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
