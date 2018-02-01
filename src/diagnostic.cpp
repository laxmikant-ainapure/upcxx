#include <upcxx/diagnostic.hpp>

#ifdef UPCXX_BACKEND
  #include <upcxx/backend_fwd.hpp>
#endif

#if UPCXX_BACKEND_GASNET
  #include <upcxx/backend/gasnet/runtime_internal.hpp>
#endif

#include <iostream>
#include <sstream>

////////////////////////////////////////////////////////////////////////

void upcxx::assert_failed(const char *file, int line, const char *msg) {
  std::stringstream ss;

  ss << std::string(50, '/') << '\n';
  ss << "UPC++ assertion failure:\n";

  #ifdef UPCXX_BACKEND
    ss << " rank=" << upcxx::backend::rank_me<<'\n';
	#endif
  
  ss << " file="<<file<<':'<<line<<'\n';
  if(msg != nullptr && '\0' != msg[0])
    ss << '\n' << msg << '\n';
  
  #if UPCXX_BACKEND_GASNET
    if(0 == gasnett_getenv_int_withdefault("GASNET_FREEZE_ON_ERROR", 0, 0)) {
      ss << "\n"
        "To have UPC++ freeze during these errors so you can attach a "
        "debugger, rerun the program with GASNET_FREEZE_ON_ERROR=1 in "
        "the environment.\n";
    }
  #endif

  ss << std::string(50, '/') << '\n';
  
  #if UPCXX_BACKEND_GASNET
    gasnett_fatalerror("\n%s", ss.str().c_str());
    //gasnett_freezeForDebuggerErr();
  #else
    std::cerr << ss.str();
    std::abort();
  #endif
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
