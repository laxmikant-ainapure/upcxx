#include <upcxx/diagnostic.hpp>

#include <iostream>
#include <sstream>

#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

namespace upcxx {
  bool dbgbrk_spin_init = false;
  bool dbgbrk_spin = true;
}

void upcxx::dbgbrk() {
  if(!dbgbrk_spin_init) {
    dbgbrk_spin_init = true;
    char *val = getenv("UPCXX_DBGBRK_SPIN");
    dbgbrk_spin = val != nullptr && val[0] != '\0' && val[0] != ' ' && val[0] != '0';
  }
  
  if(dbgbrk_spin) {
    int pid = getpid();
    
    char exe[1024]; {
      int n = readlink("/proc/self/exe", exe, sizeof(exe)-1);
      exe[n] = '\0';
    }
    
    std::stringstream ss;
    ss << "UPC++ spinning for debugger. You should:\n"
      "  gdb --pid="<<pid<<' '<<exe<<'\n'<<
      "  set dbgbrk_spin=0\n";
      //"gdb --pid="<<pid<<" -ex 'set dbgbrk_spin=0' "<<exe;
    std::cerr << ss.str();
    
    while(dbgbrk_spin)
      sched_yield();
  }
}

void upcxx::assert_failed(const char *file, int line, const char *msg) {
  std::stringstream ss;
  
  ss << "UPC++ assertion failure ["<<file<<':'<<line<<']';
  if(msg != nullptr && '\0' != msg[0])
    ss << ": " << msg;
  ss << '\n';
  
  std::cerr << ss.str();
  dbgbrk();
  std::abort();
}
