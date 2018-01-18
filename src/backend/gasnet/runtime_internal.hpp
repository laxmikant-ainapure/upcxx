#ifndef _1468755b_5808_4dd6_b81e_607919176956
#define _1468755b_5808_4dd6_b81e_607919176956

/* This header pulls in <gasnet.h> and should not be included from
 * upcxx headers that are exposed to the user.
 *
 * Including this header is the only sanctioned way to pull in the
 * gasnet API since including it is how nobs knows that it has to go
 * build gasnet before compiling the current source file.
 */

#if !NOBS_DISCOVERY // this header is a leaf-node wrt nobs discovery
  #include <upcxx/backend_fwd.hpp> // for UPCXX_BACKEND_GASNET
  
  #if UPCXX_BACKEND_GASNET
    #include <gasnet.h>

    namespace upcxx {
    namespace backend {
    namespace gasnet {
      extern gex_TM_t world_team;
    }}}
  #else
    #error "You've either pulled in this header without first including" \
           "<upcxx/backend.hpp>, or you've made the assumption that" \
           "gasnet is the desired backend (which it isn't)."
  #endif
#endif

#endif
