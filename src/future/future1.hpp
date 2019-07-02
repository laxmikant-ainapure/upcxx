#ifndef _1e7a65b7_b8d1_4def_98a3_76038c9431cf
#define _1e7a65b7_b8d1_4def_98a3_76038c9431cf

#include <upcxx/future/core.hpp>
#if UPCXX_BACKEND
  #include <upcxx/backend_fwd.hpp>
#endif

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
  
  namespace detail {
    #ifdef UPCXX_BACKEND
      struct future_wait_upcxx_progress_user {
        void operator()() const {
          UPCXX_ASSERT(
            -1 == detail::progressing(),
            "You have attempted to wait() on a non-ready future within upcxx progress, this is prohibited because it will never complete."
          );
          upcxx::progress();
        }
      };
    #endif
    
    template<typename T>
    struct is_future1: std::false_type {};
    template<typename Kind, typename ...T>
    struct is_future1<future1<Kind,T...>>: std::true_type {};
  }
  
  //////////////////////////////////////////////////////////////////////
  // future1: The actual type users get (aliased as future<>).
  
  template<typename Kind, typename ...T>
  struct future1 {
    typedef Kind kind_type;
    typedef std::tuple<T...> results_type;
    typedef typename Kind::template with_types<T...> impl_type; // impl_type is a FutureImpl.
    
    using results_rvals_type = decltype(std::declval<impl_type>().result_rvals());
    
    impl_type impl_;
    
  public:
    future1() = default;
    ~future1() = default;
    
    future1(impl_type impl): impl_(std::move(impl)) {}
    
    template<typename impl_type1,
             // Prune from overload resolution if `impl_type1` is a
             // future1 (and therefor not an actual impl type).
             typename = typename std::enable_if<!detail::is_future1<impl_type1>::value>::type>
    future1(impl_type1 impl): impl_(std::move(impl)) {}
    
    future1(future1 const&) = default;
    template<typename Kind1>
    future1(future1<Kind1,T...> const &that): impl_(that.impl_) {}
    
    future1(future1&&) = default;
    template<typename Kind1>
    future1(future1<Kind1,T...> &&that): impl_(std::move(that.impl_)) {}
    
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
  
  private:
    template<typename Tup>
    static Tup&& get_at_(Tup &&tup, std::integral_constant<int,-1>) {
      return static_cast<Tup&&>(tup);
    }
    template<typename Tup>
    static void get_at_(Tup &&tup, std::integral_constant<int,sizeof...(T)>) {
      return;
    }
    template<typename Tup, int i>
    static typename detail::tuple_element_or_void<i,Tup>::type
    get_at_(Tup const &tup, std::integral_constant<int,i>) {
      // Very odd that we need this cast since its exactly the same as the
      // return type, but we need it.
      return static_cast<typename std::tuple_element<i,Tup>::type>(std::get<i>(tup));
    }
  
  public:
    template<int i=-1>
    typename std::conditional<
        (i<0 && sizeof...(T) > 1),
          results_type,
          typename detail::tuple_element_or_void<(i<0 ? 0 : i), results_type>::type
      >::type
    result() const {
      return get_at_(
          impl_.result_lrefs_getter()(),
          std::integral_constant<int, (
              i >= (int)sizeof...(T) ? (int)sizeof...(T) :
              i>=0 ? i :
              sizeof...(T)>1 ? -1 :
              0
            )>()
        );
    }
    
    results_type result_tuple() const {
      return const_cast<impl_type&>(impl_).result_lrefs_getter()();
    }
    
    template<int i=-1>
    typename std::conditional<
        (i<0 && sizeof...(T) > 1),
          results_rvals_type,
          typename detail::tuple_element_or_void<(i<0 ? 0 : i), results_rvals_type>::type
      >::type
    result_moved() {
      return get_at_(
          impl_.result_rvals(),
          std::integral_constant<int, (
              i >= (int)sizeof...(T) ? (int)sizeof...(T) :
              i>=0 ? i :
              sizeof...(T)>1 ? -1 :
              0
            )>()
        );
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

    #ifdef UPCXX_BACKEND
    template<int i=-1, typename Fn=detail::future_wait_upcxx_progress_user>
    auto wait(Fn &&progress = detail::future_wait_upcxx_progress_user{})
    #else
    template<int i=-1, typename Fn>
    auto wait(Fn &&progress)
    #endif
      -> decltype(this->template result<i>()) {
      
      while(!impl_.ready())
        progress();
      
      return this->template result<i>();
    }
    
    #ifdef UPCXX_BACKEND
    template<typename Fn=detail::future_wait_upcxx_progress_user>
    auto wait_tuple(Fn &&progress = detail::future_wait_upcxx_progress_user{})
    #else
    template<typename Fn>
    auto wait_tuple(Fn &&progress)
    #endif
      -> decltype(this->result_tuple()) {
      
      while(!impl_.ready())
        progress();
      
      return this->result_tuple();
    }
    
    #ifdef UPCXX_BACKEND
    template<int i=-1, typename Fn=detail::future_wait_upcxx_progress_user>
    auto wait_moved(Fn &&progress = detail::future_wait_upcxx_progress_user{})
    #else
    template<int i=-1, typename Fn>
    auto wait_moved(Fn &&progress)
    #endif
      -> decltype(this->template result_moved<i>()) {
      
      while(!impl_.ready())
        progress();
      
      return this->template result_moved<i>();
    }
  };
}
#endif
