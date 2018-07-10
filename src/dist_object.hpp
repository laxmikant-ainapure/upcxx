#ifndef _60c9396d_79c1_45f4_a5d2_aa6194a75958
#define _60c9396d_79c1_45f4_a5d2_aa6194a75958

#include <upcxx/bind.hpp>
#include <upcxx/digest.hpp>
#include <upcxx/future.hpp>
#include <upcxx/rpc.hpp>
#include <upcxx/utility.hpp>

#include <cstdint>
#include <unordered_map>

namespace upcxx {
  template<typename T>
  struct dist_id;
  
  template<typename T>
  class dist_object;
  
  namespace detail {
    // Maps from `dist_id<T>` to `promise<dist_object<T>&>*`. Master
    // persona only.
    extern std::unordered_map<digest, void*> dist_master_promises;
    
    // Collective counter for generating names, should be replaced by
    // per-team counters.
    extern std::uint64_t dist_master_id_bump;
    
    // Get the promise pointer from the master map.
    template<typename T>
    promise<dist_object<T>&>* dist_promise(digest id) {
      promise<dist_object<T>&> *pro;
      auto it = dist_master_promises.find(id);
      
      if(it == dist_master_promises.end()) {
        pro = new promise<dist_object<T>&>;
        detail::dist_master_promises[id] = static_cast<void*>(pro);
      }
      else
        pro = static_cast<promise<dist_object<T>&>*>(it->second);
      
      return pro;
    }
  }
}

////////////////////////////////////////////////////////////////////////
  
namespace upcxx {
  template<typename T>
  struct dist_id {
  //private:
    digest dig_;
    
  //public:
    dist_object<T>& here() const {
      return detail::dist_promise<T>(dig_)->get_future().result();
    }
    
    future<dist_object<T>&> when_here() const {
      return detail::dist_promise<T>(dig_)->get_future();
    }
    
    #define COMPARATOR(op) \
      friend bool operator op(dist_id a, dist_id b) {\
        return a.dig_ op b.dig_; \
      }
    COMPARATOR(==)
    COMPARATOR(!=)
    COMPARATOR(<)
    COMPARATOR(<=)
    COMPARATOR(>)
    COMPARATOR(>=)
    #undef COMPARATOR
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
    digest id_;
    T value_;
    
  public:
    dist_object(T value):
      value_(std::move(value)) {
      
      // TODO: use team's collective digest generator
      id_ = {0, detail::dist_master_id_bump};
      detail::dist_master_id_bump += 1;
      
      detail::dist_promise<T>(id_)->fulfill_result(*this); // future cascade, might delete this instance!
    }
    
    dist_object(dist_object const&) = delete;
    
    dist_object(dist_object &&that):
      id_(that.id_),
      value_(std::move(that.value_)) {
      
      that.id_ = digest{~0ull, ~0ull}; // the tombstone id value
      
      promise<dist_object<T>&> *pro = new promise<dist_object<T>&>;
      
      pro->fulfill_result(*this);
      
      void *pro_void = static_cast<void*>(pro);
      std::swap(pro_void, detail::dist_master_promises[id_]);
      
      delete static_cast<promise<dist_object<T>&>*>(pro_void);
    }
    
    ~dist_object() {
      if(id_ != digest{~0ull, ~0ull}) {
        auto it = detail::dist_master_promises.find(id_);
        delete static_cast<promise<dist_object<T>&>*>(it->second);
        detail::dist_master_promises.erase(it);
      }
    }
    
    T* operator->() const { return const_cast<T*>(&value_); }
    T& operator*() const { return const_cast<T&>(value_); }
    
    dist_id<T> id() const { return dist_id<T>{id_}; }
    
    future<T> fetch(intrank_t rank) {
      return upcxx::rpc(rank, [](dist_object<T> &o) { return *o; }, *this);
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
    using stripped_type = dist_object<T>&;
    
    static dist_id<T> on_wire(dist_object<T> const &o) {
      return o.id();
    }
    
    static future<dist_object<T>&> off_wire(dist_id<T> id) {
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
