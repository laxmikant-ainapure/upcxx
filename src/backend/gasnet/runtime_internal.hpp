#ifndef _1468755b_5808_4dd6_b81e_607919176956
#define _1468755b_5808_4dd6_b81e_607919176956

/* This header pulls in <gasnet.h> and should not be included from
 * upcxx headers that are exposed to the user.
 *
 * Including this header is the only sanctioned way to pull in the
 * gasnet API since including it is how nobs knows that it has to go
 * build gasnet before compiling the current source file.
 */

#include <upcxx/backend.hpp>

#if !NOBS_DISCOVERY // this header is a leaf-node wrt nobs discovery
  #if UPCXX_BACKEND_GASNET
    #include <gasnet.h>
    #include <gasnet_coll.h>
  #else
    #error "You've either pulled in this header without first including" \
           "<upcxx/backend.hpp>, or you've made the assumption that" \
           "gasnet is the desired backend (which it isn't)."
  #endif
#endif

namespace upcxx {
namespace backend {
namespace gasnet {
  inline gex_TM_t handle_of(upcxx::team &tm) {
    return reinterpret_cast<gex_TM_t>(tm.base(detail::internal_only()).handle);
  }
  
  // Register a handle as a future with the current persona
  inline future<> register_handle_as_future(gex_Event_t h) {
    struct callback: handle_cb {
      promise<> pro;
      void execute_and_delete(handle_cb_successor) {
        backend::fulfill_during<progress_level::user>(std::move(pro), std::tuple<>());
      }
    };
    
    callback *cb = new callback;
    cb->handle = reinterpret_cast<std::uintptr_t>(h);
    get_handle_cb_queue().enqueue(cb);
    return cb->pro.get_future();
  }
}}}
#endif
