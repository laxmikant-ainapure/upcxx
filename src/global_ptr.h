/**
 * global_ptr.h
 */

#pragma once

#include <cassert> // assert
#include <cstddef> // ptrdiff_t
#include <iostream> // ostream
#include <type_traits> // is_const, is_volatile
#include "boost_utils.h" // hash_combine
#include "upcxx_runtime.h"

namespace upcxx {
  using intrank_t = int32_t; // definition of intrank_t

  // Internal PSHM functions
  bool is_memory_shared_with(intrank_t r);
  void *pshm_remote_addr2local(intrank_t r, void *addr);
  void *pshm_local_addr2remote(void *addr, intrank_t &rank_out);
 
  // Definition of global_ptr
  template<typename T>
  class global_ptr {
  public:
    static_assert(!std::is_const<T> && !std::is_volatile<T>,
                  "global_ptr<T> does not support cv qualification on T");

    using element_type = T;

    explicit global_ptr(T *_ptr) {
      if (_ptr == nullptr) {
        raw_ptr = _ptr;
        place = 0; // null pointer represented with rank 0
      } else {
        raw_ptr = pshm_local_addr2remote(_ptr, place);
        assert(raw_ptr && "address must be in shared segment");
      }
    }
 
    // null pointer represented with rank 0
    global_ptr(std::nullptr_t) : raw_ptr(nullptr), place(0) {}

    bool is_local() const {
      return is_memory_shared_with(place);
    }

    bool is_null() const {
      return raw_ptr == nullptr;
    }

    T* local() const {
      return static_cast<T*>(pshm_remote_addr2local(place, raw_ptr));
    }

    intrank_t where() const {
      return place;
    }

    global_ptr operator+(std::ptrdiff_t diff) const {
      return global_ptr(place, raw_ptr + diff);
    }

    global_ptr operator-(std::ptrdiff_t diff) const {
      return global_ptr(place, raw_ptr - diff);
    }

    std::ptrdiff_t operator-(global_ptr rhs) const {
      assert(place == rhs.place &&
             "operator- requires pointers to the same rank");
      return raw_ptr - rhs.raw_ptr;
    }

    global_ptr& operator++() {
      return *this = *this + 1;
    }

    global_ptr operator++(int) {
      global_ptr old = *this;
      *this = *this + 1;
      return old;
    }

    global_ptr& operator--() {
      return *this = *this - 1;
    }

    global_ptr operator--(int) {
      global_ptr old = *this;
      *this = *this - 1;
      return old;
    }

    bool operator==(global_ptr rhs) const {
      return place == rhs.place && raw_ptr == rhs.raw_ptr;
    }

    bool operator!=(global_ptr rhs) const {
      return place != rhs.place || raw_ptr != rhs.raw_ptr;
    }

    bool operator<(global_ptr rhs) const {
      return (place < rhs.place ||
              (place == rhs.place && raw_ptr < rhs.raw_ptr));
    }

    bool operator<=(global_ptr rhs) const {
      return (place < rhs.place ||
              (place == rhs.place && raw_ptr <= rhs.raw_ptr));
    }

    bool operator>(global_ptr rhs) const {
      return (place > rhs.place ||
              (place == rhs.place && raw_ptr > rhs.raw_ptr));
    }

    bool operator>=(global_ptr rhs) const {
      return (place > rhs.place ||
              (place == rhs.place && raw_ptr >= rhs.raw_ptr));
    }

  private:
    template<typename U>
    friend struct hash<global_ptr<U>>;

    template<typename U, typename V>
    friend global_ptr<U> reinterpret_pointer_cast(global_ptr<V> ptr);

    template<typename U>
    friend std::ostream& operator<<(std::ostream &os, global_ptr<U> ptr);

    explicit global_ptr(intrank_t place_, T* ptr_)
      : place(place_), raw_ptr(ptr_) {}

    intrank_t place;
    T* raw_ptr;
  };

  template <typename T>
  global_ptr<T> operator+(std::ptrdiff_t diff, global_ptr<T> ptr) {
    return ptr + diff;
  }

  template<typename T, typename U>
  global_ptr<T> reinterpret_pointer_cast(global_ptr<U> ptr) {
    return global_ptr<T>(ptr.place,
                         reinterpret_cast<T*>(ptr.raw_ptr));
  }

  template<typename T>
  std::ostream& operator<<(std::ostream &os, global_ptr<U> ptr) {
    return os << "(gp: " << ptr.place << ", " << ptr.raw_ptr << ")";
  }
} // namespace upcxx

// Specializations of standard function objects
namespace std {
  template<typename T>
  struct less<upcxx::global_ptr<T>> {
    constexpr bool operator()(upcxx::global_ptr<T> lhs,
                              upcxx::global_ptr<T> rhs) const {
      return lhs < rhs;
    }
  };

  template<typename T>
  struct less_equal<upcxx::global_ptr <T>> {
    constexpr bool operator()(upcxx::global_ptr<T> lhs,
                              upcxx::global_ptr<T> rhs) const {
      return lhs <= rhs;
    }
  };

  template <typename T>
  struct greater<upcxx::global_ptr <T>> {
    constexpr bool operator()(upcxx::global_ptr<T> lhs,
                              upcxx::global_ptr<T> rhs) const {
      return lhs > rhs;
    }
  };

  template<typename T>
  struct greater_equal<upcxx::global_ptr <T>> {
    constexpr bool operator()(upcxx::global_ptr<T> lhs,
                              upcxx::global_ptr<T> rhs) const {
      return lhs >= rhs;
    }
  };

  template<typename T>
  struct hash<upcxx::global_ptr<T>> {
    std::size_t operator()(upcxx::global_ptr<T> gptr) const {
      return upcxx::hash_combine(gptr.place, gptr.raw_ptr);
    }
  };
} // namespace std
