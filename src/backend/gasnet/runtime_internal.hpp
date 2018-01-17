#ifndef _1468755b_5808_4dd6_b81e_607919176956
#define _1468755b_5808_4dd6_b81e_607919176956

/* This header pulls in <gasnet.h> and should not be included from
 * upcxx headers that are exposed to the user.
 */

#include <gasnet.h>

namespace upcxx {
namespace backend {
namespace gasnet {
  extern gex_TM_t world_team;
}}}
#endif
