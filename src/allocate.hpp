#ifndef _fbcda049_eb37_438b_9a9c_a2ce81b8b8f5
#define _fbcda049_eb37_438b_9a9c_a2ce81b8b8f5

/**
 * allocate.hpp
 */

#include <upcxx/backend.hpp>
#include <upcxx/diagnostic.hpp>
#include <upcxx/global_ptr.hpp>

#include <algorithm> // max
#include <cmath> // ceil
#include <cstdint>
#include <cstddef> // max_align_t
#include <limits> // numeric_limits
#include <new> // bad_alloc
#include <type_traits> // aligned_storage, is_default_constructible,
                       // is_destructible, is_trivially_destructible

namespace upcxx {
  //////////////////////////////////////////////////////////////////////
  /* Declared in: upcxx/backend.hpp
  
  void* allocate(std::size_t size,
                 std::size_t alignment = alignof(std::max_align_t));
  
  void deallocate(void* ptr);
  */
  
  //////////////////////////////////////////////////////////////////////
  
  template<typename T, std::size_t alignment = alignof(T)>
  global_ptr<T> allocate(std::size_t n = 1) {
    void *p = upcxx::allocate(n * sizeof(T), alignment);
    return p == nullptr
      ? global_ptr<T>(nullptr)
      : global_ptr<T>(
          detail::internal_only{},
          upcxx::rank_me(),
          reinterpret_cast<T*>(p)
        );
  }

  template<typename T>
  void deallocate(global_ptr<T> gptr) {
    if (gptr != nullptr) {
      UPCXX_ASSERT(
        gptr.rank_ == upcxx::rank_me(),
        "upcxx::deallocate must be called by owner of global pointer"
      );
      
      upcxx::deallocate(gptr.raw_ptr_);
    }
  }

  namespace detail {
    template<bool throws, typename T, typename ...Args>
    global_ptr<T> new_(Args &&...args) {
      void *ptr = allocate(sizeof(T), alignof(T));
      
      if (ptr == nullptr) {
        if (throws) {
          throw std::bad_alloc();
        }
        return nullptr;
      }
      
      try {
        ::new(ptr) T(std::forward<Args>(args)...); // placement new
      } catch (...) {
        // reclaim memory and rethrow the exception
        deallocate(ptr);
        throw;
      }

      return global_ptr<T>(
        detail::internal_only{},
        upcxx::rank_me(),
        reinterpret_cast<T*>(ptr)
      );
    }
  }

  template<typename T, typename ...Args>
  global_ptr<T> new_(Args &&...args) {
    return detail::new_</*throws=*/true, T>(std::forward<Args>(args)...);
  }

  template<typename T, typename ...Args>
  global_ptr<T> new_(const std::nothrow_t &tag, Args &&...args) {
    return detail::new_</*throws=*/false, T>(std::forward<Args>(args)...);
  }

  namespace detail {
    template<bool throws, typename T>
    global_ptr<T> new_array(std::size_t n) {
      static_assert(std::is_default_constructible<T>::value,
                    "T must be default constructible");
      
      std::size_t size = sizeof(std::size_t);
      size = (size + alignof(T)-1) & -alignof(T);

      if(n > (std::numeric_limits<std::size_t>::max() - size) / sizeof(T)) {
        // more bytes required than can be represented by size_t
        if(throws)
          throw std::bad_alloc();
        return nullptr;
      }

      std::size_t offset = size;
      size += n * sizeof(T);
      
      void *ptr = upcxx::allocate(size, std::max(alignof(std::size_t), alignof(T)));
      
      if(ptr == nullptr) {
        if(throws)
          throw std::bad_alloc();
        return nullptr;
      }
      
      *reinterpret_cast<std::size_t*>(ptr) = n;
      T *elts = reinterpret_cast<T*>((char*)ptr + offset);
      
      if(!std::is_trivially_constructible<T>::value) {
        T *p = elts;
        try {
          for(T *p1=elts+n; p != p1; p++)
            ::new(p) T;
        } catch (...) {
          // destruct constructed elements, reclaim memory, and
          // rethrow the exception
          if(!std::is_trivially_destructible<T>::value) {
            for(T *p2=p-1; p2 >= elts; p2--)
              p2->~T();
          }
          deallocate(ptr);
          throw;
        }
      }
      
      return global_ptr<T>(detail::internal_only{}, upcxx::rank_me(), elts);
    }
  }

  template<typename T>
  global_ptr<T> new_array(std::size_t n) {
    return detail::new_array</*throws=*/true, T>(n);
  }

  template<typename T>
  global_ptr<T> new_array(std::size_t n, const std::nothrow_t &tag) {
    return detail::new_array</*throws=*/false, T>(n);
  }

  template<typename T>
  void delete_(global_ptr<T> gptr) {
    static_assert(std::is_destructible<T>::value,
                  "T must be destructible");
    
    if (gptr != nullptr) {
      UPCXX_ASSERT(
        gptr.rank_ == upcxx::rank_me(),
        "upcxx::delete_ must be called by owner of shared memory."
      );
      
      T *ptr = gptr.raw_ptr_;
      ptr->~T();
      upcxx::deallocate(ptr);
    }
  }

  template<typename T>
  void delete_array(global_ptr<T> gptr) {
    static_assert(std::is_destructible<T>::value,
                  "T must be destructible");
    
    if (gptr != nullptr) {
      UPCXX_ASSERT(
        gptr.rank_ == upcxx::rank_me(),
        "upcxx::delete_array must be called by owner of shared memory."
      );
      
      T *tptr = gptr.local();
      
      // padding to keep track of number of elements
      std::size_t padding = sizeof(std::size_t);
      padding = (padding + alignof(T)-1) & -alignof(T);
      
      void *ptr = reinterpret_cast<char*>(tptr) - padding;
      
      if (!std::is_trivially_destructible<T>::value) {
        std::size_t size = *reinterpret_cast<std::size_t*>(ptr);
        for(T *p=tptr, *p1=tptr+size; p != p1; p++)
          p->~T();
      }
      
      upcxx::deallocate(ptr);
    }
  }
} // namespace upcxx

#endif
