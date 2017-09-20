#include <upcxx/persona.hpp>

namespace upcxx {
  namespace detail {
    thread_local int tl_progressing = -1;
    
    thread_local persona *tl_top_persona = nullptr;
    thread_local persona tl_default_persona;
    
    thread_local persona_scope tl_default_persona_scope{tl_default_persona};
  }
}
