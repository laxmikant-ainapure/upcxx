#ifndef _eb5831b3_6325_4936_9ebb_321d97838dee
#define _eb5831b3_6325_4936_9ebb_321d97838dee

/* This header should contain the common backend API exported by all
 * upcxx backends. Some of it user-facing, some internal only.
 */

#include <upcxx/backend_fwd.hpp>

// Pulls in diagnostic.cpp which defines backend::rank_n/me
#include <upcxx/diagnostic.hpp>

#include <upcxx/future.hpp>
#include <upcxx/persona.hpp>

////////////////////////////////////////////////////////////////////////

namespace upcxx {
  persona& master_persona();
  void liberate_master_persona();
}

////////////////////////////////////////////////////////////////////////
// Backend API:

namespace upcxx {
namespace backend {
  extern persona master;
  extern persona_scope *initial_master_scope;
  
  template<typename Fn>
  void during_user(Fn &&fn);
  void during_user(promise<> &&pro);
  void during_user(promise<> *pro);
  
  template<progress_level level, typename Fn>
  void during_level(Fn &&fn);

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

namespace upcxx {
  inline persona& master_persona() {
    return upcxx::backend::master;
  }
}

////////////////////////////////////////////////////////////////////////

namespace upcxx {
namespace backend {
  template<typename Fn>
  void during_user(Fn &&fn) {
    during_level<progress_level::user>(std::forward<Fn>(fn));
  }
  
  inline void during_user(promise<> &&pro) {
    struct deferred {
      promise<> pro;
      void operator()() { pro.fulfill_result(); }
    };
    during_user(deferred{std::move(pro)});
  }
  
  inline void during_user(promise<> *pro) {
    during_user([=]() { pro->fulfill_result(); });
  }
}}
  
#endif // #ifdef guard

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
// Include backend-specific headers:

// We dispatch on backend type using preprocessor symbol which is
// defined to be another symbol that otherwise doesn't exist. So we
// define and immediately undefine them.
#define gasnet1_seq 100
#define gasnetex_par 101
#if UPCXX_BACKEND == gasnet1_seq || UPCXX_BACKEND == gasnetex_par
  #undef gasnet1_seq
  #undef gasnetex_par
  #include <upcxx/backend/gasnet/runtime.hpp>
#else
  #error "Invalid UPCXX_BACKEND."
#endif
