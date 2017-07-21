#ifndef _eb5831b3_6325_4936_9ebb_321d97838dee
#define _eb5831b3_6325_4936_9ebb_321d97838dee

/* This header should contain the common backend API exported by all
 * upcxx backends. Some of it user-facing, some internal only.
 */

#include <upcxx/future.hpp>
#include <upcxx/packing.hpp>

#include <cstddef>
#include <cstdint>

namespace upcxx {
  //////////////////////////////////////////////////////////////////////
  // User-facing:
  
  typedef int intrank_t;
  typedef unsigned int uintrank_t;
  
  enum progress_level {
    progress_level_internal,
    progress_level_user
  };
  
  void init();
  void finalize();
  
  intrank_t rank_n();
  intrank_t rank_me();
  
  void* allocate(std::size_t size,
                 std::size_t alignment = alignof(std::max_align_t));
  void deallocate(void *p);
  
  void progress(progress_level lev = progress_level_user);
  
  //////////////////////////////////////////////////////////////////////
  // Internals and implementation:
  
  namespace backend {
    extern intrank_t rank_n;
    extern intrank_t rank_me;
    
    template<progress_level level, typename Fn>
    void send_am(intrank_t recipient, Fn &&fn);
  }
  
  inline intrank_t rank_n() { return backend::rank_n; }
  inline intrank_t rank_me() { return backend::rank_me; }
}
#endif

// We dispatch on backend type using preprocessor symbol which is
// defined to be another symbol that otherwise doesn't exist. So we
// define and immediately undefine them.
#define gasnet1_seq 100
// #define gasnet1_par 101
// ...

#if UPCXX_BACKEND == gasnet1_seq
  #undef gasnet1_seq
  // #undef gasnet1_par 101
  // ...
  #include <upcxx/backend/gasnet1_seq/backend.hpp>
#else
  #error "Invalid UPCXX_BACKEND."
#endif
