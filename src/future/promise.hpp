#ifndef _502a1373_151a_4d68_96d9_32ae89053988
#define _502a1373_151a_4d68_96d9_32ae89053988

#include <upcxx/future/core.hpp>
#include <upcxx/future/impl_shref.hpp>

#include <cstdint>

namespace upcxx {
  template<typename ...T>
  class promise;
  
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
  // promise implemention
  
  template<typename ...T>
  class promise:
    private detail::future_impl_shref<
      detail::future_header_ops_result, T...
    > {
    
    std::intptr_t countdown_;
    
    promise(detail::future_header *hdr):
      detail::future_impl_shref<detail::future_header_ops_result, T...>{hdr} {
    }
    
  public:
    promise():
      detail::future_impl_shref<detail::future_header_ops_result, T...>{
        new detail::future_header_result<T...>
      },
      countdown_{1} {
    }
    
    promise(const promise&) = delete;
    promise(promise&&) = default;
    
    void require_anonymous(std::intptr_t n) {
      UPCXX_ASSERT(this->countdown_ + n > 0);
      this->countdown_ += n;
    }
    
    void fulfill_anonymous(std::intptr_t n) {
      UPCXX_ASSERT(this->countdown_ - n >= 0);
      if(0 == (this->countdown_ -= n)) {
        auto *hdr = static_cast<detail::future_header_result<T...>*>(this->hdr_);
        hdr->readify();
      }
    }
    
    void finalize_anonymous() {
      UPCXX_ASSERT(this->countdown_-1 >= 0);
      if(0 == --this->countdown_) {
        auto *hdr = static_cast<detail::future_header_result<T...>*>(this->hdr_);
        hdr->readify();
      }
    }
    
    template<typename ...U>
    void fulfill_result(U &&...values) {
      auto *hdr = static_cast<detail::future_header_result<T...>*>(this->hdr_);
      hdr->construct_results(std::forward<U>(values)...);
      if(0 == --this->countdown_)
        hdr->readify();
    }
    
    // *** not spec'd ***
    template<typename ...U>
    void fulfill_result(std::tuple<U...> &&values) {
      auto *hdr = static_cast<detail::future_header_result<T...>*>(this->hdr_);
      hdr->construct_results(std::move(values));
      if(0 == --this->countdown_)
        hdr->readify();
    }
    
    future1<
        detail::future_kind_shref<detail::future_header_ops_result>,
        T...
      >
    get_future() const {
      return static_cast<
          detail::future_impl_shref<
            detail::future_header_ops_result,
            T...
          > const&
        >(*this);
    }
  };
}  
#endif
