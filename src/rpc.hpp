#ifndef _cfdca0c3_057d_4ee9_9c82_b68e4bc96a0f
#define _cfdca0c3_057d_4ee9_9c82_b68e4bc96a0f

#include <upcxx/backend.hpp>
#include <upcxx/bind.hpp>

namespace upcxx {
  template<typename Fn, typename ...Args>
  void rpc_ff(intrank_t recipient, Fn &&fn, Args &&...args) {
    backend::template send_am<progress_level_user>(
      recipient,
      upcxx::bind(std::forward<Fn>(fn), std::forward<Args>(args)...)
    );
  }
}
#endif
