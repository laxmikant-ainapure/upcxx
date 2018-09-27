#ifndef _502a1373_151a_4d68_96d9_32ae89053988
#define _502a1373_151a_4d68_96d9_32ae89053988

#include <upcxx/future/core.hpp>
#include <upcxx/future/impl_shref.hpp>

#include <cstdint>
#include <cstddef>

namespace upcxx {
  //////////////////////////////////////////////////////////////////////
  // detail::promise_like_t: generate type promise<T...> given some
  // future<T...>
  
  namespace detail {
    template<typename Fu>
    struct promise_like;
    template<typename Kind, typename ...T>
    struct promise_like<future1<Kind,T...>> {
      using type = promise<T...>;
    };
    template<typename Fu>
    using promise_like_t = typename promise_like<Fu>::type;
  }
  
  //////////////////////////////////////////////////////////////////////
  // internal promise accessors
  
  namespace detail {
    template<typename ...T>
    promise_meta* promise_meta_of(promise<T...> &pro);
    
    template<typename ...T>
    future_header_promise<T...>* promise_header_of(promise<T...> &pro);
    
    template<typename ...T>
    future_header_promise<T...>* promise_header_of(promise_meta *meta);
  }
  
  //////////////////////////////////////////////////////////////////////
  // promise implemention
  
  template<typename ...T>
  class promise:
    private detail::future_impl_shref<
      detail::future_header_ops_promise, T...
    > {
    
    friend detail::promise_meta* detail::promise_meta_of<T...>(promise<T...>&);
    friend detail::future_header_promise<T...>* detail::promise_header_of<T...>(promise<T...> &pro);
    
    promise(detail::future_header *hdr):
      detail::future_impl_shref<detail::future_header_ops_promise, T...>(hdr) {
    }
    
  public:
    promise(std::intptr_t anons=0):
      detail::future_impl_shref<detail::future_header_ops_promise, T...>(
        &(new detail::future_header_promise<T...>)->base_header_result
      ) {
      UPCXX_ASSERT(anons >= 0);
      detail::promise_meta_of(*this)->countdown += anons;
    }
    
    promise(const promise&) = delete;
    promise(promise&&) = default;

    promise& operator=(promise &&) = default;
    
    void require_anonymous(std::intptr_t n) {
      UPCXX_ASSERT(detail::promise_meta_of(*this)->countdown > 0,
        "Called `require_anonymous()` on a ready promise.");
      UPCXX_ASSERT(detail::promise_meta_of(*this)->countdown + n > 0,
        "Calling `require_anonymous("<<n<<")` would put this promise in a ready or negative state.");
      
      detail::promise_meta_of(*this)->countdown += n;
    }
    
    void fulfill_anonymous(std::intptr_t n) {
      auto *hdr = reinterpret_cast<detail::future_header_promise<T...>*>(this->hdr_);
      hdr->fulfill(n);
    }
    
    template<typename ...U>
    void fulfill_result(U &&...values) {
      auto *hdr = reinterpret_cast<detail::future_header_promise<T...>*>(this->hdr_);
      UPCXX_ASSERT(
        !hdr->base_header_result.results_constructed(),
        "Attempted to call `fulfill_result` multiple times on the same promise."
      );
      hdr->base_header_result.construct_results(std::forward<U>(values)...);
      hdr->fulfill(1);
    }
    
    // *** not spec'd ***
    template<typename ...U>
    void fulfill_result(std::tuple<U...> &&values) {
      auto *hdr = reinterpret_cast<detail::future_header_promise<T...>*>(this->hdr_);
      UPCXX_ASSERT(
        !hdr->base_header_result.results_constructed(),
        "Attempted to call `fulfill_result` multiple times on the same promise."
      );
      hdr->base_header_result.construct_results(std::move(values));
      hdr->fulfill(1);
    }
    
    future1<
        detail::future_kind_shref<detail::future_header_ops_promise>,
        T...
      >
    finalize() {
      auto *hdr = reinterpret_cast<detail::future_header_promise<T...>*>(this->hdr_);
      hdr->fulfill(1);
      
      return static_cast<
          detail::future_impl_shref<
            detail::future_header_ops_promise,
            T...
          > const&
        >(*this);
    }
    
    future1<
        detail::future_kind_shref<detail::future_header_ops_promise>,
        T...
      >
    get_future() const {
      return static_cast<
          detail::future_impl_shref<
            detail::future_header_ops_promise,
            T...
          > const&
        >(*this);
    }
  };
  
  //////////////////////////////////////////////////////////////////////////////
  
  namespace detail {
    template<typename ...T>
    promise_meta* promise_meta_of(promise<T...> &pro) {
      return &reinterpret_cast<future_header_promise<T...>*>(pro.hdr_)->pro_meta;
    }
    
    template<typename ...T>
    future_header_promise<T...>* promise_header_of(promise<T...> &pro) {
      return reinterpret_cast<future_header_promise<T...>*>(pro.hdr_);
    }
    
    template<typename ...T>
    future_header_promise<T...>* promise_header_of(promise_meta *meta) {
      return (future_header_promise<T...>*)((char*)meta - offsetof(future_header_promise<T...>, pro_meta));
    }
  }
}
#endif
