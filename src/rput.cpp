#include <upcxx/rput.hpp>
#include <upcxx/backend/gasnet/runtime_internal.hpp>

#include <gasnetex.h>

namespace gasnet = upcxx::backend::gasnet;

template<bool source_handled, bool source_deferred_else_now>
void upcxx::detail::rma_put_nb(
    upcxx::intrank_t rank_d, void *buf_d,
    const void *buf_s, std::size_t size,
    gasnet::handle_cb *source_cb,
    gasnet::handle_cb *operation_cb
  ) {

  gex_Event_t src_h{GEX_EVENT_INVALID}, *src_ph;
  
  if(source_handled)
    src_ph = &src_h;
  else if(source_deferred_else_now)
    src_ph = GEX_EVENT_DEFER;
  else // now_source
    src_ph = GEX_EVENT_NOW;
  
  gex_Event_t op_h = gex_RMA_PutNB(
    gasnet::world_team, rank_d,
    buf_d, const_cast<void*>(buf_s), size,
    src_ph,
    /*flags*/0
  );

  if(source_handled)
    source_cb->handle = reinterpret_cast<uintptr_t>(src_h);
  
  operation_cb->handle = reinterpret_cast<uintptr_t>(op_h);
  
  gasnet::handle_cb *first = source_handled ? source_cb : operation_cb;
  gasnet::register_cb(first);

  gasnet::after_gasnet();
}

// instantiate all three cases of rma_put_nb
template
void upcxx::detail::rma_put_nb<
  /*source_handled=*/true,
  /*source_deferred_else_now=*/false
  >(
    upcxx::intrank_t rank_d, void *buf_d,
    const void *buf_s, std::size_t size,
    gasnet::handle_cb *source_cb,
    gasnet::handle_cb *operation_cb
  );

template
void upcxx::detail::rma_put_nb<
  /*source_handled=*/false,
  /*source_deferred_else_now=*/true>(
    upcxx::intrank_t rank_d, void *buf_d,
    const void *buf_s, std::size_t size,
    gasnet::handle_cb *source_cb,
    gasnet::handle_cb *operation_cb
  );

template
void upcxx::detail::rma_put_nb<
  /*source_handled=*/false,
  /*source_deferred_else_now=*/false>(
    upcxx::intrank_t rank_d, void *buf_d,
    const void *buf_s, std::size_t size,
    gasnet::handle_cb *source_cb,
    gasnet::handle_cb *operation_cb
  );

void upcxx::detail::rma_put_b(
    upcxx::intrank_t rank_d, void *buf_d,
    const void *buf_s, std::size_t size
  ) {
  
  (void)gex_RMA_PutBlocking(
    gasnet::world_team, rank_d,
    buf_d, const_cast<void*>(buf_s), size,
    /*flags*/0
  );

  gasnet::after_gasnet();
}
