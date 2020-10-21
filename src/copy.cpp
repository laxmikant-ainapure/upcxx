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

  const bool host_d = (heap_d == host_heap || heap_d == private_heap);
  const bool host_s = (heap_s == host_heap || heap_s == private_heap);

  if( host_d && host_s) { // both sides in local host memory
    UPCXX_ASSERT((char*)buf_d + size <= buf_s || (char*)buf_s + size <= buf_d,
                 "Source and destination regions in upcxx::copy must not overlap");
    std::memcpy(buf_d, buf_s, size);
    cb->execute_and_delete();
  }
  else { // one or both sides on device
  #if UPCXX_CUDA_ENABLED
    int heap_main = !host_d ? heap_d : heap_s;
    UPCXX_ASSERT(heap_main > 0);
    cuda::device_state *st = cuda::device_state::get(heap_main);
    
    CU_CHECK(cuCtxPushCurrent(st->context));

    if(!host_d && !host_s) {
      cuda::device_state *st_d = cuda::device_state::get(heap_d);
      cuda::device_state *st_s = cuda::device_state::get(heap_s);
      
      // device to device
      CU_CHECK(cuMemcpyPeerAsync(
        reinterpret_cast<CUdeviceptr>(buf_d), st_d->context,
        reinterpret_cast<CUdeviceptr>(buf_s), st_s->context,
        size, st->stream
      ));
    }
    else if(!host_d) {
      // host to device
      CU_CHECK(cuMemcpyHtoDAsync(reinterpret_cast<CUdeviceptr>(buf_d), buf_s, size, st->stream));
    }
    else {
      UPCXX_ASSERT(!host_s);
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
