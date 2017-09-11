#ifndef _eb5831b3_6325_4936_9ebb_321d97838dee
#define _eb5831b3_6325_4936_9ebb_321d97838dee

/* This header should contain the common backend API exported by all
 * upcxx backends. Some of it user-facing, some internal only.
 */

#include <upcxx/diagnostic.hpp> // for upcxx::backend::rank_n/me
#include <upcxx/future.hpp>
#include <upcxx/packing.hpp>

#include <cstddef>
#include <cstdint>

//////////////////////////////////////////////////////////////////////
// Public API:
  
namespace upcxx {
  typedef int intrank_t;
  typedef unsigned int uintrank_t;
  
  enum class progress_level {
    internal,
    user
  };
  
  void init();
  void finalize();
  
  intrank_t rank_n();
  intrank_t rank_me();
  
  void* allocate(std::size_t size,
                 std::size_t alignment = alignof(std::max_align_t));
  void deallocate(void *p);
  
  void progress(progress_level lev = progress_level::user);
  
  void barrier();
}

////////////////////////////////////////////////////////////////////////
// Backend API:

namespace upcxx {
namespace backend {
  // These are actually defined in diagnostic.cpp so that asserts can
  // print the current rank without pulling in the backend for non-parallel
  // programs.
  extern intrank_t rank_n;
  extern intrank_t rank_me;
  
  template<typename Fn>
  void during_user(Fn &&fn);
  void during_user(promise<> &&pro);
  void during_user(promise<> *pro);
  
  template<typename Fn>
  void during_level(progress_level level, Fn &&fn);

  template<progress_level level, typename Fn>
  void send_am(intrank_t recipient, Fn &&fn);
  
  // Type definitions provided by backend.
  struct rma_put_cb;
  template<typename State>
  struct rma_put_cb_wstate; // derives rma_put_cb
  
  template<typename State, typename SrcCx, typename OpCx>
  rma_put_cb_wstate<State>* make_rma_put_cb(State state, SrcCx src_cx, OpCx op_cx);
  
  void rma_put(
    intrank_t rank_d,
    void *buf_d,
    void *buf_s,
    std::size_t buf_size,
    rma_put_cb *cb
  );
  
  // Type definitions provided by backend.
  struct rma_get_cb;
  template<typename State>
  struct rma_get_cb_wstate; // derives rma_get_cb
  
  template<typename State, typename OpCx>
  rma_get_cb_wstate<State>* make_rma_get_cb(State state, OpCx op_cx);
  
  void rma_get(
    void *buf_d,
    intrank_t rank_s,
    void *buf_s,
    std::size_t buf_size,
    rma_get_cb *cb
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
