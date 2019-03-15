#include <upcxx/upcxx.hpp>
#include <upcxx/os_env.hpp>
#include <gasnet.h>

#include <cstdlib>
#include <iostream>

int main(int argc, char **argv) {
  upcxx::init();
  const char *key = "UPCXX_SEGMENT_MB";
  if (argc > 1)  key = argv[1];

  const char *p =  std::getenv(key);
  const char *g =  gasnett_getenv(key);
  std::string u =  upcxx::os_env<std::string>(key,"NULL");
  std::ostringstream oss;
  oss << upcxx::rank_me() << ":" 
      << " std::getenv(" << key << ")=" << (p?p:"NULL") << " \t"
      << " gasnett_getenv(" << key << ")=" << (g?g:"NULL") << " \t"
      << " os_env(" << key << ")=" << u
      << "\n";
  std::cout << oss.str() << std::flush;

  upcxx::finalize();
  return 0;
}
