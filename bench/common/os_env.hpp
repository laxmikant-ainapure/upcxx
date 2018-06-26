#ifndef _325f0701_4dd7_4cfe_9b01_996de4980b99
#define _325f0701_4dd7_4cfe_9b01_996de4980b99

#include <upcxx/diagnostic.hpp>

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

namespace bench {
  template<class T>
  T os_env(const std::string &name);
  template<class T>
  T os_env(const std::string &name, const T &otherwise);

  namespace detail {
    template<class T>
    struct os_env_parse;
    
    template<>
    struct os_env_parse<std::string> {
      std::string operator()(std::string x) {
        return x;
      }
    };
    
    template<class T>
    struct os_env_parse {
      T operator()(std::string x) {
        std::istringstream ss{x};
        T val;
        ss >> val;
        return val;
      }
    };
  }
}

template<class T>
T bench::os_env(const std::string &name) {
  std::string sval;
  
  char const *p = std::getenv(name.c_str());
  UPCXX_ASSERT(p != 0x0);
  sval = p;
  
  return detail::os_env_parse<T>()(std::move(sval));
}

template<class T>
T bench::os_env(const std::string &name, const T &otherwise) {
  std::string sval;
  
  char const *p = std::getenv(name.c_str());
  if(p) sval = p;
  
  if(sval.size() != 0)
    return detail::os_env_parse<T>()(std::move(sval));
  else
    return otherwise;
}

#endif
