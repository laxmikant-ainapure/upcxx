#include <upcxx/cuda.hpp>
#include <upcxx/cuda_internal.hpp>

namespace detail = upcxx::detail;

using std::size_t;

#if UPCXX_CUDA_ENABLED
  namespace {
    detail::segment_allocator make_segment(CUcontext cxt, void *base, size_t size) {
      CU_CHECK(cuCtxPushCurrent(cxt));
      
      CUdeviceptr p = 0x0;

      if(-size == 1) {
        size_t lo=1<<20, hi=size_t(16)<<30;
        
        while(hi-lo > 64<<10) {
          if(p) cuMemFree(p);
          size = (lo + hi)/2;
          CUresult r = cuMemAlloc(&p, size);

          if(r == CUDA_ERROR_OUT_OF_MEMORY)
            hi = size;
          else if(r == CUDA_SUCCESS)
            lo = size;
          else
            CU_CHECK(r);
        }

        base = reinterpret_cast<void*>(p);
      }
      else if(base == nullptr) {
        CUresult r = cuMemAlloc(&p, size);
        UPCXX_ASSERT_ALWAYS(r != CUDA_ERROR_OUT_OF_MEMORY, "Requested cuda allocation too large: size="<<size);
        CU_CHECK(r);
        base = reinterpret_cast<void*>(p);
      }

      CU_CHECK(cuCtxPopCurrent(&cxt));
      
      return detail::segment_allocator(base, size);
    }
  }

  upcxx::cuda::device_state *upcxx::cuda::devices[upcxx::cuda::max_devices] = {/*nullptr...*/};
#endif

upcxx::cuda_device::cuda_device(int device):
  device_(device) {

  UPCXX_ASSERT_ALWAYS(backend::master.active_with_caller());

  #if UPCXX_CUDA_ENABLED
    if(device != invalid_device_id) {
      UPCXX_ASSERT_ALWAYS(cuda::devices[device] == nullptr, "Cuda device "<<device<<" already initialized.");
      
      CUcontext ctx;
      CUresult res = cuDevicePrimaryCtxRetain(&ctx, device);
      if(res == CUDA_ERROR_NOT_INITIALIZED) {
        cuInit(0);
        res = cuDevicePrimaryCtxRetain(&ctx, device);
      }
      CU_CHECK(res);
      CU_CHECK(cuCtxPushCurrent(ctx));

      cuda::device_state *st = new cuda::device_state;
      st->context = ctx;
      CU_CHECK(cuStreamCreate(&st->stream, CU_STREAM_NON_BLOCKING));
      cuda::devices[device] = st;
      
      CU_CHECK(cuCtxPopCurrent(&ctx));
    }
  #else
    UPCXX_ASSERT_ALWAYS(device == invalid_device_id);
  #endif
}

upcxx::cuda_device::~cuda_device() {
  UPCXX_ASSERT_ALWAYS(device_ == invalid_device_id, "upcxx::cuda_device must have destroy() called before it dies.");
}

void upcxx::cuda_device::destroy(upcxx::entry_barrier eb) {
  UPCXX_ASSERT(backend::master.active_with_caller());

  #if UPCXX_CUDA_ENABLED
  if(device_ != invalid_device_id) {
    backend::quiesce(upcxx::world(), eb);

    cuda::device_state *st = cuda::devices[device_];
    UPCXX_ASSERT(st != nullptr);
    cuda::devices[device_] = nullptr;

    CU_CHECK(cuCtxPushCurrent(st->context));
    CU_CHECK(cuStreamDestroy(st->stream));
    CU_CHECK(cuCtxSetCurrent(nullptr));
    CU_CHECK(cuDevicePrimaryCtxRelease(device_));
    
    delete st;
  }
  #endif
  
  device_ = invalid_device_id;
}

detail::device_allocator_core<upcxx::cuda_device>::device_allocator_core(
    upcxx::cuda_device &dev, void *base, size_t size
  ):
  detail::device_allocator_base(
    dev.device_,
    #if UPCXX_CUDA_ENABLED
      make_segment(cuda::devices[dev.device_]->context, base, size)
    #else
      segment_allocator(nullptr, 0)
    #endif
  ),
  free_on_death_(base == nullptr) {
}

detail::device_allocator_core<upcxx::cuda_device>::~device_allocator_core() {
#if UPCXX_CUDA_ENABLED
  if(free_on_death_) {
    CU_CHECK(cuCtxPushCurrent(cuda::devices[device_]->context));

    cuMemFree(reinterpret_cast<CUdeviceptr>(this->seg_.segment_range().first));
    
    CUcontext dump;
    CU_CHECK(cuCtxPopCurrent(&dump));
  }
#endif
}
