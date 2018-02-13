#ifndef _eb5831b3_6325_4936_9ebb_321d97838dee
#define _eb5831b3_6325_4936_9ebb_321d97838dee

/* This header should contain the common backend API exported by all
 * upcxx backends. Some of it user-facing, some internal only.
 */

#include <upcxx/backend_fwd.hpp>
#include <upcxx/future.hpp>
#include <upcxx/persona.hpp>

////////////////////////////////////////////////////////////////////////

namespace upcxx {
  persona& master_persona();
  void liberate_master_persona();
  
  bool progress_required(persona_scope &ps = top_persona_scope());
  void discharge(persona_scope &ps = top_persona_scope());
}

////////////////////////////////////////////////////////////////////////
// Backend API:

namespace upcxx {
namespace backend {
  extern persona master;
  extern persona_scope *initial_master_scope;
  
  template<typename Fn>
  void during_user(Fn &&fn);
  template<typename ...T>
  void during_user(promise<T...> &&pro, T ...vals);
  template<typename ...T>
  void during_user(promise<T...> &pro, T ...vals);
  
  template<progress_level level, typename Fn>
  void during_level(Fn &&fn);

  template<progress_level level, typename Fn>
  void send_am_master(intrank_t recipient, Fn &&fn);
  
  template<progress_level level, typename Fn>
  void send_am_persona(intrank_t recipient_rank, persona *recipient_persona, Fn &&fn);
}}

////////////////////////////////////////////////////////////////////////

namespace upcxx {
  inline persona& master_persona() {
    return upcxx::backend::master;
  }
  
  inline bool progress_required(persona_scope&) {
    return false;
  }
  
  inline void discharge(persona_scope &ps) {
    while(upcxx::progress_required(ps))
      upcxx::progress(progress_level::internal);
  }
}

////////////////////////////////////////////////////////////////////////

namespace upcxx {
namespace backend {
  template<typename Fn>
  void during_user(Fn &&fn) {
    during_level<progress_level::user>(std::forward<Fn>(fn));
  }
  
  template<typename ...T>
  void during_user(promise<T...> &&pro, T ...vals) {
    struct deferred {
      promise<T...> pro;
      std::tuple<T...> vals;
      void operator()() { pro.fulfill_result(vals); }
    };
    
    during_user(
      deferred{
        std::move(pro),
        std::tuple<T...>{std::move(vals)...}
      }
    );
  }
  
  template<typename ...T>
  void during_user(promise<T...> &pro, T ...vals) {
    struct deferred {
      promise<T...> *pro;
      std::tuple<T...> vals;
      void operator()() { pro->fulfill_result(vals); }
    };
    
    during_user(
      deferred{
        &pro,
        std::tuple<T...>{std::move(vals)...}
      }
    );
  }
}}
  
#endif // #ifdef guard

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
// Include backend-specific headers:

#if UPCXX_BACKEND_GASNET_SEQ || UPCXX_BACKEND_GASNET_PAR
  #include <upcxx/backend/gasnet/runtime.hpp>
#elif !defined(NOBS_DISCOVERY)
  #error "Invalid UPCXX_BACKEND."
#endif
