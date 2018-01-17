#include <upcxx/rget.hpp>
#include <upcxx/backend/gasnet/runtime_internal.hpp>
#include <gasnetex.h>

namespace gasnet = upcxx::backend::gasnet;

void upcxx::detail::rma_get_nb(
    void *buf_d,
    intrank_t rank_s,
    const void *buf_s,
    std::size_t buf_size,
    gasnet::handle_cb *cb
  ) {

  gex_Event_t h = gex_RMA_GetNB(
    gasnet::world_team,
    buf_d, rank_s, const_cast<void*>(buf_s), buf_size,
    /*flags*/0
  );
  cb->handle = reinterpret_cast<uintptr_t>(h);
  
  gasnet::register_cb(cb);
  gasnet::after_gasnet();
}

void upcxx::detail::rma_get_b(
    void *buf_d,
    intrank_t rank_s,
    const void *buf_s,
    std::size_t buf_size
  ) {

  (void)gex_RMA_GetBlocking(
    gasnet::world_team,
    buf_d, rank_s, const_cast<void*>(buf_s), buf_size,
    /*flags*/0
  );
  
  gasnet::after_gasnet();
}
