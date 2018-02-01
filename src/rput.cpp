#include <upcxx/rput.hpp>
#include <upcxx/backend/gasnet/runtime_internal.hpp>

namespace gasnet = upcxx::backend::gasnet;

template<upcxx::detail::rma_put_source_mode source_mode>
void upcxx::detail::rma_put_nb(
    upcxx::intrank_t rank_d, void *buf_d,
    const void *buf_s, std::size_t size,
    gasnet::handle_cb *source_cb,
    gasnet::handle_cb *operation_cb
  ) {

  gex_Event_t src_h{GEX_EVENT_INVALID}, *src_ph;

  switch(source_mode) {
  case rma_put_source_mode::handle:
    src_ph = &src_h;
    break;
  case rma_put_source_mode::defer:
    src_ph = GEX_EVENT_DEFER;
    break;
  default: // rma_put_source_mode::now:
    src_ph = GEX_EVENT_NOW;
    break;
  }
  
  gex_Event_t op_h = gex_RMA_PutNB(
    gasnet::world_team, rank_d,
    buf_d, const_cast<void*>(buf_s), size,
    src_ph,
    /*flags*/0
  );

  if(source_mode == rma_put_source_mode::handle)
    source_cb->handle = reinterpret_cast<uintptr_t>(src_h);
  
  operation_cb->handle = reinterpret_cast<uintptr_t>(op_h);
  
  gasnet::handle_cb *first =
    source_mode == rma_put_source_mode::handle
      ? source_cb
      : operation_cb;
  gasnet::register_cb(first);

  gasnet::after_gasnet();
}

// instantiate all three cases of rma_put_nb
template
void upcxx::detail::rma_put_nb<
  /*source_mode=*/upcxx::detail::rma_put_source_mode::handle
  >(
    upcxx::intrank_t rank_d, void *buf_d,
    const void *buf_s, std::size_t size,
    gasnet::handle_cb *source_cb,
    gasnet::handle_cb *operation_cb
  );

template
void upcxx::detail::rma_put_nb<
  /*source_mode=*/upcxx::detail::rma_put_source_mode::defer
  >(
    upcxx::intrank_t rank_d, void *buf_d,
    const void *buf_s, std::size_t size,
    gasnet::handle_cb *source_cb,
    gasnet::handle_cb *operation_cb
  );

template
void upcxx::detail::rma_put_nb<
  /*source_mode=*/upcxx::detail::rma_put_source_mode::now
  >(
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
