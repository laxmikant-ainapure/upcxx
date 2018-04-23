#include <upcxx/persona.hpp>

namespace upcxx {
  namespace detail {
    __thread persona_tls the_persona_tls{};
  }
}
