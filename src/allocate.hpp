#pragma once

/**
 * allocate.hpp
 */

#include <algorithm> // max
#include <cassert> // assert
#include <cmath> // ceil
#include <cstddef> // max_align_t
#include <new> // bad_alloc
#include <type_traits> // aligned_storage, is_default_constructible,
                       // is_destructible, is_trivially_destructible
#include "global_ptr.hpp"

namespace upcxx {
  void* allocate(size_t size,
                 size_t alignment = alignof(std::max_align_t));

  template<typename T, size_t alignment = alignof(T)>
  global_ptr<T> allocate(size_t n = 1) {
    return allocate(n * sizeof(typename std::aligned_storage<sizeof(T),
                                                             alignment>::type),
                    alignment);
  }

  void deallocate(void* ptr);

  template<typename T>
  void deallocate(global_ptr<T> gptr) {
    if (gptr != nullptr) {
      //assert(gptr.where() == rank_me() &&
      //       "deallocate must be called by owner of global pointer");
      deallocate(gptr.local());
    }
  }

  template<typename T, typename ...Args>
  global_ptr<T> _new_(bool throws, Args &&...args) {
    void *ptr = allocate(sizeof(T), alignof(T));
    if (ptr == nullptr) {
      if (throws) {
        throw std::bad_alloc();
      }
      return nullptr;
    }
    new(ptr) T(std::forward<Args>(args)...); // placement new
    return global_ptr<T>(reinterpret_cast<T*>(ptr));
  }

  template<typename T, typename ...Args>
  global_ptr<T> new_(Args &&...args) {
    _new_<T>(true, std::forward<Args>(args)...);
  }

  template<typename T, typename ...Args>
  global_ptr<T> new_(const std::nothrow_t &tag, Args &&...args) {
    _new_<T>(false, std::forward<Args>(args)...);
  }

  template<typename T>
  global_ptr<T> _new_array(size_t n, bool throws) {
    static_assert(std::is_default_constructible<T>::value,
                  "T must be default constructible");
    // padding for storage to keep track of number of elements
    size_t padding = std::max(alignof(size_t),
                              std::max(alignof(T), sizeof(size_t)));
    void *ptr = allocate(n * sizeof(T) + padding);
    if (ptr == nullptr) {
      if (throws) {
        throw std::bad_alloc();
      }
      return nullptr;
    }
    *(reinterpret_cast<size_t*>(ptr)) = n; // store size for deallocation
    T *tptr = reinterpret_cast<T*>(reinterpret_cast<char*>(ptr) +
                                   padding); // ptr to actual data
    new(tptr) T[n]; // array placement new
    return global_ptr<T>(tptr);
  }

  template<typename T>
  global_ptr<T> new_array(size_t n) {
    return _new_array<T>(n, true);
  }

  template<typename T>
  global_ptr<T> new_array(size_t n, const std::nothrow_t &tag) {
    return _new_array<T>(n, false);
  }

  template<typename T>
  void delete_(global_ptr<T> gptr) {
    static_assert(std::is_destructible<T>::value,
                  "T must be destructible");
    if (gptr != nullptr) {
      //assert(gptr.where() == rank_me() &&
      //       "delete_ must be called by owner of global pointer");
      T *ptr = gptr.local();
      ptr->~T();
      deallocate(ptr);
    }
  }

  template<typename T>
  void delete_array(global_ptr<T> gptr) {
    static_assert(std::is_destructible<T>::value,
                  "T must be destructible");
    if (gptr != nullptr) {
      //assert(gptr.where() == rank_me() &&
      //       "delete_array must be called by owner of global pointer");
      T *tptr = gptr.local();
      // padding to keep track of number of elements
      size_t padding = std::max(alignof(size_t),
                                std::max(alignof(T), sizeof(size_t)));
      void *ptr = reinterpret_cast<char*>(tptr) - padding;
      if (!std::is_trivially_destructible<T>::value) {
        size_t size = *reinterpret_cast<size_t*>(ptr);
        for (size_t i = 0; i < size; ++i) {
          tptr[i].~T(); // destroy each element
        }
      }
      deallocate(ptr);
    }
  }
} // namespace upcxx
