#include <upcxx/diagnostic.hpp>

#ifdef UPCXX_BACKEND
  #include <upcxx/backend_fwd.hpp>
#endif

#include <iostream>
#include <sstream>

#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

////////////////////////////////////////////////////////////////////////

#if UPCXX_BACKEND_GASNET
  #include <gasnetex.h>
  #include <gasnet_tools.h>

  extern "C" {
    volatile int upcxx_frozen;
  }

  void upcxx::dbgbrk() {
    gasnett_freezeForDebuggerNow(&upcxx_frozen, "upcxx_frozen");
  }
#else
  void upcxx::dbgbrk() {}
#endif

void upcxx::assert_failed(const char *file, int line, const char *msg) {
  std::stringstream ss;

  ss << "UPC++ assertion failure";

  #ifdef UPCXX_BACKEND
    ss << " on rank " << upcxx::backend::rank_me;
	#endif
  
  ss << " ["<<file<<':'<<line<<']';
  if(msg != nullptr && '\0' != msg[0])
    ss << ": " << msg;
  ss << '\n';
  
  std::cerr << ss.str();
  dbgbrk();
  std::abort();
}

upcxx::say::say() {
  #ifdef UPCXX_BACKEND
    ss << '[' << upcxx::backend::rank_me << "] ";
  #endif
}

upcxx::say::~say() {
  ss << std::endl;
  std::cerr << ss.str();
}
