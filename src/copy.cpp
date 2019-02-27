#include <upcxx/copy.hpp>
#include <upcxx/cuda_internal.hpp>
#include <upcxx/backend/gasnet/runtime_internal.hpp>

using namespace std;

namespace detail = upcxx::detail;
namespace gasnet = upcxx::backend::gasnet;
namespace cuda = upcxx::cuda;

using upcxx::memory_kind;
using upcxx::detail::lpc_base;

void upcxx::detail::rma_copy_cuda(
    int dev_d, void *buf_d,
    int dev_s, void const *buf_s, std::size_t size,
    cuda::event_cb *cb
  ) {

// this function is no-op without cuda since it won't ever be called
#if UPCXX_CUDA_ENABLED

  constexpr int host_device = -1;
  
  int dev_main = dev_d != host_device ? dev_d : dev_s;
  cuda::device_state *st = static_cast<cuda::device_state*>(cuda::devices[dev_main]);
  
  CU_CHECK(cuCtxPushCurrent(st->context));

  if(dev_d != host_device && dev_s != host_device) {
    // device to device
    CURT_CHECK(cudaMemcpyPeerAsync(
      buf_d, dev_d,
      buf_s, dev_s,
      size, st->stream));
  }
  else {
    if(dev_d != host_device) {
      // host to device
      CU_CHECK(cuMemcpyHtoDAsync(reinterpret_cast<CUdeviceptr>(buf_d), buf_s, size, st->stream));
    }
    else {
      // device to host
      CU_CHECK(cuMemcpyDtoHAsync(buf_d, reinterpret_cast<CUdeviceptr>(buf_s), size, st->stream));
    }
  }

  CUevent event;
  cuEventCreate(&event, CU_EVENT_DISABLE_TIMING);
  CU_CHECK(cuEventRecord(event, st->stream));
  cb->cu_event = (void*)event;

  persona *per = detail::the_persona_tls.get_top_persona();
  per->cuda_state_.event_cbs.enqueue(cb);
  
  {CUcontext dump; CU_CHECK(cuCtxPopCurrent(&dump));}
#endif
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
