#ifndef _eb5831b3_6325_4936_9ebb_321d97838dee
#define _eb5831b3_6325_4936_9ebb_321d97838dee

/* This header should contain the common backend API exported by all
 * upcxx backends. Some of it user-facing, some internal only.
 */

#include <upcxx/backend_fwd.hpp>
#include <upcxx/future.hpp>
#include <upcxx/persona.hpp>

#include <cstdint>
#include <memory>
#include <tuple>

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

  //////////////////////////////////////////////////////////////////////

  // inclusive lower and exclusive upper bounds for local_team ranks
  extern intrank_t pshm_peer_lb, pshm_peer_ub, pshm_peer_n;
  
  // Given index in local_team:
  //   local_minus_remote: Encodes virtual address translation which is added
  //     to the raw encoding to get local virtual address.
  //   vbase: Local virtual address mapping to beginning of peer's segment
  //   size: Size of peer's segment in bytes.
  extern std::unique_ptr<std::uintptr_t[/*local_team.size()*/]> pshm_local_minus_remote;
  extern std::unique_ptr<std::uintptr_t[/*local_team.size()*/]> pshm_vbase;
  extern std::unique_ptr<std::uintptr_t[/*local_team.size()*/]> pshm_size;

  inline bool rank_is_local(intrank_t r) {
    return std::uintptr_t(r) - std::uintptr_t(pshm_peer_lb) < std::uintptr_t(pshm_peer_n);
    // Is equivalent to...
    // return pshm_peer_lb <= r && r < pshm_peer_ub;
  }
  
  void* localize_memory(intrank_t rank, std::uintptr_t raw);
  void* localize_memory_nonnull(intrank_t rank, std::uintptr_t raw);
  
  std::tuple<intrank_t/*rank*/, std::uintptr_t/*raw*/> globalize_memory(void const *addr);
  std::uintptr_t globalize_memory_nonnull(intrank_t rank, void const *addr);
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
  void fulfill_during_user(promise<T...> &pro, std::tuple<T...> vals) {
    auto &tls = detail::the_persona_tls;
    tls.fulfill_during_user_of_top(pro, std::move(vals));
  }
  template<typename ...T>
  void fulfill_during_user(promise<T...> &pro, std::intptr_t anon) {
    auto &tls = detail::the_persona_tls;
    tls.fulfill_during_user_of_top(pro, anon);
  }
  
  template<typename ...T>
  void fulfill_during_user(promise<T...> &&pro, std::tuple<T...> vals) {
    auto &tls = detail::the_persona_tls;
    tls.fulfill_during_user_of_top(std::move(pro), std::move(vals));
  }
  template<typename ...T>
  void fulfill_during_user(promise<T...> &&pro, std::intptr_t anon) {
    auto &tls = detail::the_persona_tls;
    tls.fulfill_during_user_of_top(std::move(pro), anon);
  }
  
  inline void* localize_memory_nonnull(intrank_t rank, std::uintptr_t raw) {
    UPCXX_ASSERT(
      pshm_peer_lb <= rank && rank < pshm_peer_ub,
      "Rank "<<rank<<" is not local with current rank ("<<upcxx::rank_me()<<")."
    );

    intrank_t peer = rank - pshm_peer_lb;
    std::uintptr_t u = raw + pshm_local_minus_remote[peer];

    UPCXX_ASSERT(
      u - pshm_vbase[peer] < pshm_size[peer], // unsigned arithmetic handles both sides of the interval test
      "Memory address (raw="<<raw<<", local="<<reinterpret_cast<void*>(u)<<") is not within shared segment of rank "<<rank<<"."
    );

    return reinterpret_cast<void*>(u);
  }
  
  inline void* localize_memory(intrank_t rank, uintptr_t raw) {
    if(raw == reinterpret_cast<uintptr_t>(nullptr))
      return nullptr;
    
    return localize_memory_nonnull(rank, raw);
  }
  
  inline std::uintptr_t globalize_memory_nonnull(intrank_t rank, void const *addr) {
    UPCXX_ASSERT(
      pshm_peer_lb <= rank && rank < pshm_peer_ub,
      "Rank "<<rank<<" is not local with current rank ("<<upcxx::rank_me()<<")."
    );
    
    std::uintptr_t u = reinterpret_cast<std::uintptr_t>(addr);
    intrank_t peer = rank - pshm_peer_lb;
    std::uintptr_t raw = u - pshm_local_minus_remote[peer];
    
    UPCXX_ASSERT(
      u - pshm_vbase[peer] < pshm_size[peer], // unsigned arithmetic handles both sides of the interval test
      "Memory address (raw="<<raw<<", local="<<addr<<") is not within shared segment of rank "<<rank<<"."
    );
    
    return raw;
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
