#include <upcxx/cuda.hpp>
#include <upcxx/cuda_internal.hpp>

namespace detail = upcxx::detail;

using std::size_t;

constexpr std::size_t detail::device_allocator_core<upcxx::cuda_device>::min_alignment; // required before C++17

#if UPCXX_CUDA_ENABLED
  namespace {
    detail::segment_allocator make_segment(upcxx::cuda::device_state *st, void *base, size_t size) {
      CU_CHECK(cuCtxPushCurrent(st->context));
      
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

        st->segment_to_free = p;
        base = reinterpret_cast<void*>(p);
      }
      else if(base == nullptr) {
        CUresult r = cuMemAlloc(&p, size);
        UPCXX_ASSERT_ALWAYS(r != CUDA_ERROR_OUT_OF_MEMORY, "Requested cuda allocation too large: size="<<size);
        CU_CHECK(r);
        
        st->segment_to_free = p;
        base = reinterpret_cast<void*>(p);
      }
      else
        st->segment_to_free = reinterpret_cast<CUdeviceptr>(nullptr);
      
      CUcontext dump;
      CU_CHECK(cuCtxPopCurrent(&dump));
      
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
      UPCXX_ASSERT_ALWAYS(res == CUDA_SUCCESS, "cuDevicePrimaryCtxRetain failed, error="<<int(res));
      CU_CHECK_ALWAYS(cuCtxPushCurrent(ctx));

      cuda::device_state *st = new cuda::device_state;
      st->context = ctx;
      CU_CHECK_ALWAYS(cuStreamCreate(&st->stream, CU_STREAM_NON_BLOCKING));
      cuda::devices[device] = st;
      
      CU_CHECK_ALWAYS(cuCtxPopCurrent(&ctx));
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

  backend::quiesce(upcxx::world(), eb);

  #if UPCXX_CUDA_ENABLED
  if(device_ != invalid_device_id) {
    cuda::device_state *st = cuda::devices[device_];
    UPCXX_ASSERT(st != nullptr);
    cuda::devices[device_] = nullptr;

    if(st->segment_to_free)
      CU_CHECK_ALWAYS(cuMemFree(st->segment_to_free));
    
    CU_CHECK_ALWAYS(cuCtxPushCurrent(st->context));
    CU_CHECK_ALWAYS(cuStreamDestroy(st->stream));
    CU_CHECK_ALWAYS(cuCtxSetCurrent(nullptr));
    CU_CHECK_ALWAYS(cuDevicePrimaryCtxRelease(device_));
    
    delete st;
  }
  #endif
  
  device_ = invalid_device_id;
}

detail::device_allocator_core<upcxx::cuda_device>::device_allocator_core(
    upcxx::cuda_device *dev, void *base, size_t size
  ):
  detail::device_allocator_base(
    dev ? dev->device_ : upcxx::cuda_device::invalid_device_id,
    #if UPCXX_CUDA_ENABLED
      dev ? make_segment(cuda::devices[dev->device_], base, size)
          : segment_allocator(nullptr, 0)
    #else
      segment_allocator(nullptr, 0)
    #endif
  ) {
  UPCXX_ASSERT_ALWAYS(backend::master.active_with_caller());
}

detail::device_allocator_core<upcxx::cuda_device>::~device_allocator_core() {
  UPCXX_ASSERT_ALWAYS(backend::master.active_with_caller());
  
  #if UPCXX_CUDA_ENABLED  
    if(device_ != upcxx::cuda_device::invalid_device_id) {
      cuda::device_state *st = cuda::devices[device_];
      
      if(st && st->segment_to_free) {
        CU_CHECK_ALWAYS(cuCtxPushCurrent(st->context));
        CU_CHECK_ALWAYS(cuMemFree(st->segment_to_free));
        CUcontext dump;
        CU_CHECK_ALWAYS(cuCtxPopCurrent(&dump));

        st->segment_to_free = reinterpret_cast<CUdeviceptr>(nullptr);
      }
    }
  #endif
}
