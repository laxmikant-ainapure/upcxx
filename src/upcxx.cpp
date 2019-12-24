/* This is stub file for generating libupcxx.a. The transitive header
 * crawl done in nobsrule is how all the cpp file will be found.
 */
#include <upcxx/upcxx.hpp>
#include <upcxx/upcxx_internal.hpp>

namespace upcxx {
  // these must be runtime functions compiled into the library, to comply with xSDK community policy M8
  long release_version() { return UPCXX_VERSION; }
  long spec_version()    { return UPCXX_SPEC_VERSION; }
};

