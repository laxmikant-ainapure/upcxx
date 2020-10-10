#include <upcxx/copy.hpp>
#include <upcxx/cuda_internal.hpp>
#include <upcxx/backend/gasnet/runtime_internal.hpp>

#include <cstring>

using namespace std;

namespace detail = upcxx::detail;
namespace gasnet = upcxx::backend::gasnet;
namespace cuda = upcxx::cuda;

using upcxx::memory_kind;
using upcxx::detail::lpc_base;

void upcxx::detail::rma_copy_local(
    int heap_d, void *buf_d,
    int heap_s, void const *buf_s, std::size_t size,
    cuda::event_cb *cb
  ) {

  if(heap_d == host_heap && heap_s == host_heap) {
    std::memmove(buf_d, buf_s, size);
    cb->execute_and_delete();
  }
  else {
  #if UPCXX_CUDA_ENABLED
    int heap_main = heap_d != host_heap ? heap_d : heap_s;
    cuda::device_state *st = cuda::device_state::get(heap_main);
    
    CU_CHECK(cuCtxPushCurrent(st->context));

    if(heap_d != host_heap && heap_s != host_heap) {
      cuda::device_state *st_d = cuda::device_state::get(heap_d);
      cuda::device_state *st_s = cuda::device_state::get(heap_s);
      
      // device to device
      CU_CHECK(cuMemcpyPeerAsync(
        reinterpret_cast<CUdeviceptr>(buf_d), st_d->context,
        reinterpret_cast<CUdeviceptr>(buf_s), st_s->context,
        size, st->stream
      ));
    }
    else if(heap_d != host_heap) {
      // host to device
      CU_CHECK(cuMemcpyHtoDAsync(reinterpret_cast<CUdeviceptr>(buf_d), buf_s, size, st->stream));
    }
    else {
      // device to host
      CU_CHECK(cuMemcpyDtoHAsync(buf_d, reinterpret_cast<CUdeviceptr>(buf_s), size, st->stream));
    }

    CUevent event;
    CU_CHECK(cuEventCreate(&event, CU_EVENT_DISABLE_TIMING));
    CU_CHECK(cuEventRecord(event, st->stream));
    cb->cu_event = (void*)event;

    persona *per = detail::the_persona_tls.get_top_persona();
    per->cuda_state_.event_cbs.enqueue(cb);
    
    {CUcontext dump; CU_CHECK(cuCtxPopCurrent(&dump));}
  #else
    UPCXX_FATAL_ERROR("Unrecognized heaps in upcxx::copy() -- gptr corruption?");
  #endif
  }
}

void upcxx::detail::rma_copy_get(
    void *buf_d, intrank_t rank_s, void const *buf_s, std::size_t size,
    gasnet::handle_cb *cb
  ) {
  gex_Event_t h = gex_RMA_GetNB(
    gasnet::handle_of(upcxx::world()),
    buf_d, rank_s, const_cast<void*>(buf_s), size,
    /*flags*/0
  );
  cb->handle = reinterpret_cast<uintptr_t>(h);
  gasnet::register_cb(cb);
  gasnet::after_gasnet();
}

void upcxx::detail::rma_copy_put(
    intrank_t rank_d, void *buf_d, void const *buf_s, std::size_t size,
    gasnet::handle_cb *cb
  ) {
  gex_Event_t h = gex_RMA_PutNB(
    gasnet::handle_of(upcxx::world()),
    rank_d, buf_d, const_cast<void*>(buf_s), size,
    GEX_EVENT_DEFER,
    /*flags*/0
  );
  cb->handle = reinterpret_cast<uintptr_t>(h);
  gasnet::register_cb(cb);
  gasnet::after_gasnet();
}
