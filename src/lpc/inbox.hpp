#ifndef _2d897dc6_1696_4bd9_b530_6d923356fa84
#define _2d897dc6_1696_4bd9_b530_6d923356fa84

#ifndef UPCXX_LPC_INBOX_locked
  #define UPCXX_LPC_INBOX_locked 0
#endif

#ifndef UPCXX_LPC_INBOX_lockfree
  #define UPCXX_LPC_INBOX_lockfree 0
#endif

#ifndef UPCXX_LPC_INBOX_syncfree
  #define UPCXX_LPC_INBOX_syncfree 0
#endif

#include <upcxx/diagnostic.hpp>
#include <upcxx/lpc/inbox_syncfree.hpp>

#include <atomic>
#include <mutex>

namespace upcxx {
  // This type is contained within `__thread` storage, so it must be:
  //   1. trivially destructible.
  //   2. constexpr constructible equivalent to zero-initialization.
  template<int queue_n, bool thread_safe>
  struct lpc_inbox;
  
  template<int queue_n>
  struct lpc_inbox<queue_n, /*thread_safe=*/false>:
    upcxx::lpc_inbox_syncfree<queue_n> {
  };
}

#if UPCXX_LPC_INBOX_locked
  #include <upcxx/lpc/inbox_locked.hpp>
  namespace upcxx {
    template<int queue_n>
    struct lpc_inbox<queue_n, /*thread_safe=*/true>:
      upcxx::lpc_inbox_locked<queue_n> {
    };
  }

#elif UPCXX_LPC_INBOX_lockfree
  #include <upcxx/lpc/inbox_lockfree.hpp>
  namespace upcxx {
    template<int queue_n>
    struct lpc_inbox<queue_n, /*thread_safe=*/true>:
      upcxx::lpc_inbox_lockfree<queue_n> {
    };
  }
  
#elif UPCXX_LPC_INBOX_syncfree
  namespace upcxx {
    template<int queue_n>
    struct lpc_inbox<queue_n, /*thread_safe=*/true>:
      upcxx::lpc_inbox_syncfree<queue_n> {
    };
  }
  
#elif !defined(NOBS_DISCOVERY)
  #error "Invalid UPCXX_LPC_INBOX_IMPL."
#endif

#endif
