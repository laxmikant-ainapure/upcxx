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

  namespace detail {
    // internal_only: Type used to mark member functions of public types for
    // internal use only. To do so, just make one of the function's arguments
    // of this type. The caller can just pass a default constructed dummy value.
    struct internal_only {};
  }
  
  void init();
  void finalize();
  
  intrank_t rank_n();
  intrank_t rank_me();
  
  void* allocate(std::size_t size,
                 std::size_t alignment = alignof(std::max_align_t));
  void deallocate(void *p);
  
  void progress(progress_level level = progress_level::user);
  
  void barrier();
  
  namespace detail {
    int progressing();
  }
}

////////////////////////////////////////////////////////////////////////
// Backend API:

namespace upcxx {
namespace backend {
  extern int init_count;
  extern intrank_t rank_n;
  extern intrank_t rank_me;
}}


#if UPCXX_BACKEND_GASNET_PAR
  #include <upcxx/backend/gasnet/handle_cb.hpp>
  
  namespace upcxx {
  namespace backend {
    // This type is contained within `__thread` storage, so it must be:
    //   1. trivially destructible.
    //   2. constexpr constructible equivalent to zero-initialization.
    struct persona_state {
      // personas carry their list of oustanding gasnet handles
      gasnet::handle_cb_queue hcbs;
    };
  }}
#else
  namespace upcxx {
  namespace backend {
    // This type is contained within `__thread` storage, so it must be:
    //   1. trivially destructible.
    //   2. constexpr constructible equivalent to zero-initialization.
    struct persona_state {
      // personas carry no extra state
    };
  }}
#endif

////////////////////////////////////////////////////////////////////////
// Public API implementations:

namespace upcxx {
  inline intrank_t rank_n() { return backend::rank_n; }
  inline intrank_t rank_me() { return backend::rank_me; }
}

#endif
