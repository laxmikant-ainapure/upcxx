#ifndef _677c2cf9_fcbb_4243_bd62_8138df3c50f7
#define _677c2cf9_fcbb_4243_bd62_8138df3c50f7

#include <upcxx/backend.hpp>
#include <upcxx/future.hpp>

namespace upcxx {
  template<typename Kind, typename ...T>
  auto wait(future1<Kind, T...> const &f)
    -> decltype(f.result()) {
    
    while(!f.ready())
      upcxx::progress();
    
    return f.result();
  }
}
#endif
