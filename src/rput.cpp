#include <upcxx/rput.hpp>
#include <upcxx/backend/gasnet/runtime_internal.hpp>

namespace gasnet = upcxx::backend::gasnet;
namespace detail = upcxx::detail;

template<detail::rma_put_mode mode>
detail::rma_put_done detail::rma_put(
    upcxx::intrank_t rank_d, void *buf_d,
    const void *buf_s, std::size_t size,
    gasnet::handle_cb *source_cb,
    gasnet::handle_cb *operation_cb
  ) {

  if(mode != rma_put_mode::op_now) {
    gex_Event_t src_h = GEX_EVENT_INVALID, *src_ph;

    switch(mode) {
    case rma_put_mode::src_handle:
      src_ph = &src_h;
      break;
    case rma_put_mode::src_defer:
      src_ph = GEX_EVENT_DEFER;
      break;
    case rma_put_mode::src_now:
    default:
      src_ph = GEX_EVENT_NOW;
      break;
    }
    
    gex_Event_t op_h = gex_RMA_PutNB(
      gasnet::handle_of(upcxx::world()), rank_d,
      buf_d, const_cast<void*>(buf_s), size,
      src_ph,
      /*flags*/0
    );
    
    operation_cb->handle = reinterpret_cast<uintptr_t>(op_h);
    
    if(mode == rma_put_mode::src_handle)
      source_cb->handle = reinterpret_cast<uintptr_t>(src_h);
    
    if(0 == gex_Event_Test(op_h))
      return rma_put_done::operation;
    
    if(mode == rma_put_mode::src_now)
      return rma_put_done::source;
    
    if(mode == rma_put_mode::src_handle && 0 == gex_Event_Test(src_h))
      return rma_put_done::source;
    
    return rma_put_done::none;
  }
  else {
    (void)gex_RMA_PutBlocking(
      gasnet::handle_of(upcxx::world()), rank_d,
      buf_d, const_cast<void*>(buf_s), size,
      /*flags*/0
    );
    
    return rma_put_done::operation;
  }
}

// instantiate all four cases of rma_put
template
detail::rma_put_done detail::rma_put<
  /*mode=*/detail::rma_put_mode::src_handle
  >(
    upcxx::intrank_t rank_d, void *buf_d,
    const void *buf_s, std::size_t size,
    gasnet::handle_cb *source_cb,
    gasnet::handle_cb *operation_cb
  );

template
detail::rma_put_done detail::rma_put<
  /*mode=*/detail::rma_put_mode::src_defer
  >(
    upcxx::intrank_t rank_d, void *buf_d,
    const void *buf_s, std::size_t size,
    gasnet::handle_cb *source_cb,
    gasnet::handle_cb *operation_cb
  );

template
detail::rma_put_done detail::rma_put<
  /*mode=*/detail::rma_put_mode::src_now
  >(
    upcxx::intrank_t rank_d, void *buf_d,
    const void *buf_s, std::size_t size,
    gasnet::handle_cb *source_cb,
    gasnet::handle_cb *operation_cb
  );

template
detail::rma_put_done detail::rma_put<
  /*mode=*/detail::rma_put_mode::op_now
  >(
    upcxx::intrank_t rank_d, void *buf_d,
    const void *buf_s, std::size_t size,
    gasnet::handle_cb *source_cb,
    gasnet::handle_cb *operation_cb
  );
