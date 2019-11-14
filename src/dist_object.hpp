#ifndef _60c9396d_79c1_45f4_a5d2_aa6194a75958
#define _60c9396d_79c1_45f4_a5d2_aa6194a75958

#include <upcxx/bind.hpp>
#include <upcxx/digest.hpp>
#include <upcxx/future.hpp>
#include <upcxx/rpc.hpp>
#include <upcxx/utility.hpp>
#include <upcxx/team.hpp>

#include <cstdint>
#include <functional>

namespace upcxx {
  template<typename T>
  struct dist_id;
  
  template<typename T>
  class dist_object;
}

////////////////////////////////////////////////////////////////////////
  
namespace upcxx {
  template<typename T>
  struct dist_id {
  //private:
    digest dig_;
    
  //public:
    dist_object<T>& here() const {
      return detail::registered_promise<dist_object<T>&>(dig_)->get_future().result();
    }
    
    future<dist_object<T>&> when_here() const {
      return detail::promise_get_future(detail::registered_promise<dist_object<T>&>(dig_));
    }
    
    #define UPCXX_COMPARATOR(op) \
      friend bool operator op(dist_id a, dist_id b) {\
        return a.dig_ op b.dig_; \
      }
    UPCXX_COMPARATOR(==)
    UPCXX_COMPARATOR(!=)
    UPCXX_COMPARATOR(<)
    UPCXX_COMPARATOR(<=)
    UPCXX_COMPARATOR(>)
    UPCXX_COMPARATOR(>=)
    #undef UPCXX_COMPARATOR
  };
  
  template<typename T>
  std::ostream& operator<<(std::ostream &o, dist_id<T> x) {
    return o << x.dig_;
  }
}

namespace std {
  template<typename T>
  struct hash<upcxx::dist_id<T>> {
    size_t operator()(upcxx::dist_id<T> id) const {
      return hash<upcxx::digest>()(id.dig_);
    }
  };
}

////////////////////////////////////////////////////////////////////////

namespace upcxx {
  template<typename T>
  class dist_object {
    upcxx::team *tm_;
    digest id_;
    T value_;
    
  public:
    template<typename ...U>
    dist_object(upcxx::team &tm, U &&...arg):
      tm_(&tm),
      value_(std::forward<U>(arg)...) {
      
      id_ = tm.next_collective_id(detail::internal_only());
      
      backend::fulfill_during<progress_level::user>(
          detail::registered_promise<dist_object<T>&>(id_)->incref(1),
          std::tuple<dist_object<T>&>(*this),
          backend::master
        );
    }
    
    dist_object(T value, upcxx::team &tm):
      tm_(&tm),
      value_(std::move(value)) {
      
      id_ = tm.next_collective_id(detail::internal_only());
      
      backend::fulfill_during<progress_level::user>(
          detail::registered_promise<dist_object<T>&>(id_)->incref(1),
          std::tuple<dist_object<T>&>(*this),
          backend::master
        );
    }
    
    dist_object(T value):
      dist_object(upcxx::world(), std::move(value)) {
    }
    
    dist_object(dist_object const&) = delete;
    
    dist_object(dist_object &&that) noexcept:
      tm_(that.tm_),
      id_(that.id_),
      value_(std::move(that.value_)) {
      
      that.id_ = digest{~0ull, ~0ull}; // the tombstone id value
      
      auto *pro = new detail::future_header_promise<dist_object<T>&>;
      detail::promise_fulfill_result(pro, *this);
      
      void *pro_void = static_cast<void*>(pro);
      std::swap(pro_void, detail::registry[id_]);
      
      static_cast<detail::future_header_promise<dist_object<T>&>*>(pro_void)->dropref();
    }
    
    ~dist_object() {
      if(id_ != digest{~0ull, ~0ull}) {
        auto it = detail::registry.find(id_);
        static_cast<detail::future_header_promise<dist_object<T>&>*>(it->second)->dropref();
        detail::registry.erase(it);
      }
    }
    
    T* operator->() const { return const_cast<T*>(&value_); }
    T& operator*() const { return const_cast<T&>(value_); }
    
    upcxx::team& team() const { return *tm_; }
    dist_id<T> id() const { return dist_id<T>{id_}; }
    
    future<T> fetch(intrank_t rank) const {
      return upcxx::rpc(*tm_, rank, [](dist_object<T> const &o) { return *o; }, *this);
    }
  };
}

////////////////////////////////////////////////////////////////////////

namespace upcxx {
  // dist_object<T> references are bound using their id's.
  template<typename T>
  struct binding<dist_object<T>&> {
    using on_wire_type = dist_id<T>;
    using off_wire_type = dist_object<T>&;
    using off_wire_future_type = future<dist_object<T>&>;
    using stripped_type = dist_object<T>&;
    static constexpr bool immediate = false;
    
    static dist_id<T> on_wire(dist_object<T> const &o) {
      return o.id();
    }
    
    static future<dist_object<T>&> off_wire(dist_id<T> id) {
      return id.when_here();
    }
    static future<dist_object<T>&> off_wire_future(dist_id<T> id) {
      return id.when_here();
    }
  };
  
  template<typename T>
  struct binding<dist_object<T> const&>:
    binding<dist_object<T>&> {
    
    using stripped_type = dist_object<T> const&;
  };
  
  template<typename T>
  struct binding<dist_object<T>&&> {
    static_assert(sizeof(T) != sizeof(T),
      "Moving a dist_object into a binding must surely be an error!"
    );
  };
}
#endif
