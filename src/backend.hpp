#ifndef _eb5831b3_6325_4936_9ebb_321d97838dee
#define _eb5831b3_6325_4936_9ebb_321d97838dee

/* This header should contain the common backend API exported by all
 * upcxx backends. Some of it user-facing, some internal only.
 */

#include <upcxx/future.hpp>
#include <upcxx/packing.hpp>

#include <cstddef>
#include <cstdint>

//////////////////////////////////////////////////////////////////////
// Public API:
  
namespace upcxx {
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
}

////////////////////////////////////////////////////////////////////////
// Backend API:

namespace upcxx {
namespace backend {
  extern intrank_t rank_n;
  extern intrank_t rank_me;
  
  template<progress_level level, typename Fn>
  void send_am(intrank_t recipient, Fn &&fn);
  
  struct rma_callback {
    virtual void fire_and_delete() = 0;
  };
  
  template<typename Fn>
  rma_callback* make_rma_cb(Fn fn);
  
  // Do a GET, execute `done` callback in `done_level` progress when finished.
  // (xxx_d = dest, xxx_s = source)
  void rma_get(
    void *buf_d,
    intrank_t rank_s,
    void *buf_s,
    std::size_t buf_size,
    progress_level done_level,
    rma_callback *done
  );
  
  // Do a PUT, execute `done` callback in `done_level` progress when finished.
  // (xxx_d = dest, xxx_s = source)
  void rma_put(
    intrank_t rank_d,
    void *buf_d,
    void *buf_s,
    std::size_t buf_size,
    progress_level done_level,
    rma_callback *done
  );
}}
  
////////////////////////////////////////////////////////////////////////
// Public API implementations:

namespace upcxx {
  inline intrank_t rank_n() { return backend::rank_n; }
  inline intrank_t rank_me() { return backend::rank_me; }
}

#endif // #ifdef guard

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
// Include backend-specific headers:

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
