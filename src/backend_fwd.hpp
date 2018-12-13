#ifndef _f93ccf7a_35a8_49c6_b7b2_55c3c1a9640c
#define _f93ccf7a_35a8_49c6_b7b2_55c3c1a9640c

#ifndef UPCXX_BACKEND_GASNET_SEQ
  #define UPCXX_BACKEND_GASNET_SEQ 0
#endif

#ifndef UPCXX_BACKEND_GASNET_PAR
  #define UPCXX_BACKEND_GASNET_PAR 0
#endif

#define UPCXX_BACKEND_GASNET (UPCXX_BACKEND_GASNET_SEQ | UPCXX_BACKEND_GASNET_PAR)

/* This header declares some core user-facing API to break include
 * cycles with headers included by the real "backend.hpp". This header
 * does not pull in the implementation of what it exposes, so you can't
 * use anything that has a runtime analog unless guarded by a:
 *   #ifdef UPCXX_BACKEND
 */

#include <upcxx/future/fwd.hpp>

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

  enum class entry_barrier {
    none,
    internal,
    user
  };
  
  namespace detail {
    // internal_only: Type used to mark member functions of public types for
    // internal use only. To do so, just make one of the function's arguments
    // of this type. The caller can just pass a default constructed dummy value.
    struct internal_only {
      explicit constexpr internal_only() {}
    };
  }
    
  class persona;
  class persona_scope;
  class team;
  
  void init();
  bool initialized();
  void finalize();

  void destroy_heap();
  void restore_heap();
  
  intrank_t rank_n();
  intrank_t rank_me();
  
  void* allocate(std::size_t size,
                 std::size_t alignment = alignof(std::max_align_t));
  void deallocate(void *p);
  
  void progress(progress_level level = progress_level::user);
  
  persona& master_persona();
  void liberate_master_persona();
  
  persona& default_persona();
  persona& current_persona();
  persona_scope& default_persona_scope();
  persona_scope& top_persona_scope();
  
  bool progress_required(persona_scope &ps = top_persona_scope());
  void discharge(persona_scope &ps = top_persona_scope());
  
  namespace detail {
    int progressing();
  }
}

////////////////////////////////////////////////////////////////////////////////
// Backend API:

#if UPCXX_BACKEND_GASNET_PAR
  #include <upcxx/backend/gasnet/handle_cb.hpp>
#endif

namespace upcxx {
namespace backend {
  extern int init_count;
  extern intrank_t rank_n;
  extern intrank_t rank_me;
  extern bool verbose_noise;
  
  extern persona master;
  extern persona_scope *initial_master_scope;
  
  struct team_base; // backend-specific base class for teams (holds a handle usually)
  
  // This type is contained within `__thread` storage, so it must be:
  //   1. trivially destructible.
  //   2. constexpr constructible equivalent to zero-initialization.
  struct persona_state {
    #if UPCXX_BACKEND_GASNET_PAR
      // personas carry their list of oustanding gasnet handles
      gasnet::handle_cb_queue hcbs;
    #else
      // personas carry no extra state
    #endif
  };
  
  void quiesce(team &tm, entry_barrier eb);
  
  template<progress_level level, typename Fn>
  void during_level(Fn &&fn, persona &active_per = current_persona());
  
  template<typename Fn>
  void during_user(Fn &&fn, persona &active_per = current_persona());
  
  template<progress_level level, typename ...T>
  void fulfill_during(
      promise<T...> &pro, std::tuple<T...> vals,
      persona &active_per = current_persona()
    );
  template<progress_level level, typename ...T>
  void fulfill_during(
      promise<T...> &pro, std::intptr_t anon,
      persona &active_per = current_persona()
    );
  
  template<progress_level level, typename ...T>
  void fulfill_during(
      promise<T...> &&pro, std::tuple<T...> vals,
      persona &active_per = current_persona()
    );
  template<progress_level level, typename ...T>
  void fulfill_during(
      promise<T...> &&pro, std::intptr_t anon,
      persona &active_per = current_persona()
    );
  
  template<progress_level level, typename Fn>
  void send_am_master(team &tm, intrank_t recipient, Fn &&fn);
  
  template<progress_level level, typename Fn>
  void send_am_persona(team &tm, intrank_t recipient_rank, persona *recipient_persona, Fn &&fn);

  template<progress_level level, typename Fn>
  void bcast_am_master(team &tm, Fn &&fn);
  
  intrank_t team_rank_from_world(team &tm, intrank_t rank);
  intrank_t team_rank_from_world(team &tm, intrank_t rank, intrank_t otherwise);
  intrank_t team_rank_to_world(team &tm, intrank_t peer);
  
  bool rank_is_local(intrank_t r);
  
  void* localize_memory(intrank_t rank, std::uintptr_t raw);
  void* localize_memory_nonnull(intrank_t rank, std::uintptr_t raw);
  
  std::tuple<intrank_t/*rank*/, std::uintptr_t/*raw*/> globalize_memory(void const *addr);
  std::tuple<intrank_t/*rank*/, std::uintptr_t/*raw*/> globalize_memory(void const *addr, std::tuple<intrank_t,std::uintptr_t> otherwise);
  std::uintptr_t globalize_memory_nonnull(intrank_t rank, void const *addr);
}}

////////////////////////////////////////////////////////////////////////
// Public API implementations:

namespace upcxx {
  inline intrank_t rank_n() { return backend::rank_n; }
  inline intrank_t rank_me() { return backend::rank_me; }
}

////////////////////////////////////////////////////////////////////////////////
// Include backend-specific headers:

#if UPCXX_BACKEND_GASNET_SEQ || UPCXX_BACKEND_GASNET_PAR
  #include <upcxx/backend/gasnet/runtime_fwd.hpp>
#endif

#endif
