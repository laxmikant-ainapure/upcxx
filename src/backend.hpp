#ifndef _eb5831b3_6325_4936_9ebb_321d97838dee
#define _eb5831b3_6325_4936_9ebb_321d97838dee

#include <upcxx/backend_fwd.hpp>
#include <upcxx/future.hpp>
#include <upcxx/persona.hpp>
#include <upcxx/team.hpp>

#include <cstdint>
#include <memory>
#include <tuple>

////////////////////////////////////////////////////////////////////////////////

namespace upcxx {
  inline bool initialized() {
    return backend::init_count != 0;
  }
  
  inline persona& master_persona() {
    return backend::master;
  }
  
  inline bool progress_required(persona_scope&) {
    return false;
  }
  
  inline void discharge(persona_scope &ps) {
    while(upcxx::progress_required(ps))
      upcxx::progress(progress_level::internal);
  }
}

////////////////////////////////////////////////////////////////////////////////
// upcxx::backend implementation: non-backend specific

namespace upcxx {
namespace backend {
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

  //////////////////////////////////////////////////////////////////////////////
  
  template<typename Fn>
  void during_user(Fn &&fn, persona &active_per) {
    during_level<progress_level::user>(std::forward<Fn>(fn), active_per);
  }
  
  //////////////////////////////////////////////////////////////////////////////
  // fulfill_during_<level=internal>
  
  template<typename ...T>
  void fulfill_during_(
      std::integral_constant<progress_level, progress_level::internal>,
      promise<T...> &pro, std::tuple<T...> vals,
      persona &active_per
    ) {
  
    struct fulfiller {
      promise<T...> &pro;
      std::tuple<T...> vals;
      void operator()() {
        pro.fulfill_result(std::move(vals));
      }
    };
    
    auto &tls = detail::the_persona_tls;
    tls.during(active_per, progress_level::internal, fulfiller{pro, std::move(vals)}, /*known_active=*/std::true_type());
  }
  
  template<typename ...T>
  void fulfill_during_(
      std::integral_constant<progress_level, progress_level::internal>,
      promise<T...> &pro, std::intptr_t anon,
      persona &active_per
    ) {
  
    auto &tls = detail::the_persona_tls;
    tls.during(active_per, progress_level::internal, [=,&pro]() { pro.fulfill_anonymous(anon); }, /*known_active=*/std::true_type());
  }
  
  template<typename ...T>
  void fulfill_during_(
      std::integral_constant<progress_level, progress_level::internal>,
      promise<T...> &&pro, std::tuple<T...> vals,
      persona &active_per
    ) {
    struct fulfiller {
      promise<T...> pro;
      std::tuple<T...> vals;
      void operator()() {
        pro.fulfill_result(std::move(vals));
      }
    };
    
    auto &tls = detail::the_persona_tls;
    tls.during(active_per, progress_level::internal, fulfiller{std::move(pro), std::move(vals)}, /*known_active=*/std::true_type());
  }
  template<typename ...T>
  void fulfill_during_(
      std::integral_constant<progress_level, progress_level::internal>,
      promise<T...> &&pro, std::intptr_t anon,
      persona &active_per
    ) {
    struct fulfiller {
      promise<T...> pro;
      std::intptr_t anon;
      void operator()() {
        pro.fulfill_anonymous(anon);
      }
    };
    
    auto &tls = detail::the_persona_tls;
    tls.during(active_per, progress_level::internal, fulfiller{std::move(pro), anon}, /*known_active=*/std::true_type());
  }
  
  //////////////////////////////////////////////////////////////////////////////
  // fulfill_during_<level=user>
  
  template<typename Pro, typename ...T>
  void fulfill_during_(
      std::integral_constant<progress_level, progress_level::user>,
      Pro &&pro, std::tuple<T...> vals,
      persona &active_per
    ) {
    auto &tls = detail::the_persona_tls;
    tls.fulfill_during_user_of_active(active_per, std::forward<Pro>(pro), std::move(vals));
  }
  template<typename Pro>
  void fulfill_during_(
      std::integral_constant<progress_level, progress_level::user>,
      Pro &&pro, std::intptr_t anon,
      persona &active_per
    ) {
    auto &tls = detail::the_persona_tls;
    tls.fulfill_during_user_of_active(active_per, std::forward<Pro>(pro), anon);
  }
  
  //////////////////////////////////////////////////////////////////////////////
  
  template<progress_level level, typename ...T>
  void fulfill_during(
      promise<T...> &pro, std::tuple<T...> vals,
      persona &active_per
    ) {
    fulfill_during_(
        std::integral_constant<progress_level,level>(),
        pro, std::move(vals), active_per
      );
  }
  template<progress_level level, typename ...T>
  void fulfill_during(
      promise<T...> &pro, std::intptr_t anon,
      persona &active_per
    ) {
    fulfill_during_(
        std::integral_constant<progress_level,level>(),
        pro, anon, active_per
      );
  }
  
  template<progress_level level, typename ...T>
  void fulfill_during(
      promise<T...> &&pro, std::tuple<T...> vals,
      persona &active_per
    ) {
    fulfill_during_(
        std::integral_constant<progress_level,level>(),
        std::move(pro), std::move(vals), active_per
      );
  }
  template<progress_level level, typename ...T>
  void fulfill_during(
      promise<T...> &&pro, std::intptr_t anon,
      persona &active_per
    ) {
    fulfill_during_(
        std::integral_constant<progress_level,level>(),
        std::move(pro), anon, active_per
      );
  }
  
  //////////////////////////////////////////////////////////////////////////////
  
  inline bool rank_is_local(intrank_t r) {
    return std::uintptr_t(r) - std::uintptr_t(pshm_peer_lb) < std::uintptr_t(pshm_peer_n);
    // Is equivalent to...
    // return pshm_peer_lb <= r && r < pshm_peer_ub;
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
  
  inline void* localize_memory(intrank_t rank, std::uintptr_t raw) {
    if(raw == reinterpret_cast<std::uintptr_t>(nullptr))
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
  
////////////////////////////////////////////////////////////////////////////////
// Include backend-specific headers:

#if UPCXX_BACKEND_GASNET_SEQ || UPCXX_BACKEND_GASNET_PAR
  #include <upcxx/backend/gasnet/runtime.hpp>
#elif !defined(NOBS_DISCOVERY)
  #error "Invalid UPCXX_BACKEND."
#endif

#endif // #ifdef guard
