#include <upcxx/cuda.hpp>
#include <upcxx/cuda_internal.hpp>
#include <upcxx/reduce.hpp>

namespace detail = upcxx::detail;

using std::size_t;

#if UPCXX_CUDA_ENABLED
namespace {
  detail::segment_allocator make_segment(int heap_idx, void *base, size_t size) {
    upcxx::cuda::device_state *st = heap_idx <= 0 ? nullptr :
                                    upcxx::cuda::device_state::get(heap_idx);
    bool throw_bad_alloc = false;

    if (st) { // creating a real device heap, possibly allocating memory

      CU_CHECK(cuCtxPushCurrent(st->context));
      
      CUdeviceptr p = 0x0;

      if(-size == 1) { // undocumented "largest available"
        size_t lo=1<<20, hi=size_t(16)<<30;
        
        while(hi-lo > 64<<10) {
          if(p) CU_CHECK_ALWAYS(cuMemFree(p));
          size = (lo + hi)/2;
          CUresult r = cuMemAlloc(&p, size);

          if(r == CUDA_ERROR_OUT_OF_MEMORY)
            hi = size;
          else if(r == CUDA_SUCCESS)
            lo = size;
          else
            CU_CHECK_ALWAYS(r);
        }

        st->segment_to_free = p;
        base = reinterpret_cast<void*>(p);

      } else if(base == nullptr) { // allocate a particular size
        CUresult r = cuMemAlloc(&p, size);
        if(r == CUDA_ERROR_OUT_OF_MEMORY) {
          throw_bad_alloc = true;
          p = reinterpret_cast<CUdeviceptr>(nullptr);
        } else
          UPCXX_ASSERT_ALWAYS(r == CUDA_SUCCESS, "Requested cuda allocation failed: size="<<size<<", return="<<int(r));
        
        st->segment_to_free = p;
        base = reinterpret_cast<void*>(p);

      } else { // client-provided segment
        st->segment_to_free = reinterpret_cast<CUdeviceptr>(nullptr);
      }
      
      CUcontext dump;
      CU_CHECK(cuCtxPopCurrent(&dump));
    }

    // once segment is collectively created, decide whether we are keeping it.
    // this is a effectively a user-level barrier, but not a documented guarantee
    throw_bad_alloc = upcxx::reduce_all<bool>(throw_bad_alloc, upcxx::op_fast_bit_or).wait();

    if (throw_bad_alloc) { // single-valued
      // at least one process failed to allocate, so collectively unwind and throw
      if (st && st->segment_to_free) {
        CU_CHECK(cuCtxPushCurrent(st->context));
        CU_CHECK_ALWAYS(cuMemFree(st->segment_to_free));
        CUcontext dump;
        CU_CHECK(cuCtxPopCurrent(&dump));
        st->segment_to_free = reinterpret_cast<CUdeviceptr>(nullptr);
      }
      // TODO: collect and report information about the failing request in exn.what()
      throw std::bad_alloc();
    } else {
      // TODO: wire-up the GASNet segment
      //   gex_Segment_Create()
      //   gex_EP_BindSegment()
      //   gex_EP_PublishBoundSegment()
      return detail::segment_allocator(base, size);
    }
  } // make_segment

  detail::device_allocator_core<upcxx::cuda_device> tombstone;
} // anon namespace
#endif

#if UPCXX_CUDA_ENABLED
void upcxx::cuda::cu_failed(CUresult res, const char *file, int line, const char *expr) {
  const char *errname, *errstr;
  cuGetErrorName(res, &errname);
  cuGetErrorString(res, &errstr);
  
  std::stringstream ss;
  ss << expr <<"\n  error="<<errname<<": "<<errstr;
  
  upcxx::fatal_error(ss.str(), "CUDA call failed", nullptr, file, line);
}

void upcxx::cuda::curt_failed(cudaError_t res, const char *file, int line, const char *expr) {
  const char *errname, *errstr;
  errname = cudaGetErrorName(res);
  errstr = cudaGetErrorString(res);
  
  std::stringstream ss;
  ss << expr <<"\n  error="<<errname<<": "<<errstr;
  
  upcxx::fatal_error(ss.str(), "CUDA call failed", nullptr, file, line);
}
#endif

upcxx::cuda_device::cuda_device(int device):
  device_(device), heap_idx_(-1) {

  UPCXX_ASSERT_INIT();
  UPCXX_ASSERT_ALWAYS_MASTER();
  UPCXX_ASSERT_COLLECTIVE_SAFE(entry_barrier::user);

  #if UPCXX_CUDA_ENABLED
    if (device != invalid_device_id) {
      heap_idx_ = backend::heap_state::alloc_index();
      CUcontext ctx;
      CUresult res = cuDevicePrimaryCtxRetain(&ctx, device);
      if(res == CUDA_ERROR_NOT_INITIALIZED) {
        cuInit(0);
        res = cuDevicePrimaryCtxRetain(&ctx, device);
      }
      CU_CHECK_ALWAYS(("cuDevicePrimaryCtxRetain()", res));
      CU_CHECK_ALWAYS(cuCtxPushCurrent(ctx));

      cuda::device_state *st = new cuda::device_state{};
      st->context = ctx;
      st->device_id = device;
      st->alloc_base = nullptr;
      st->segment_to_free = reinterpret_cast<CUdeviceptr>(nullptr);

      // TODO: gex_EP_Create
      st->ep_index = heap_idx_;
      
      CU_CHECK_ALWAYS(cuStreamCreate(&st->stream, CU_STREAM_NON_BLOCKING));
      backend::heap_state::get(heap_idx_,true) = st;
      
      CU_CHECK_ALWAYS(cuCtxPopCurrent(&ctx));
    }
  #else
    UPCXX_ASSERT_ALWAYS(device == invalid_device_id);
  #endif
}

upcxx::cuda_device::~cuda_device() {
  if(backend::init_count > 0) { // we don't assert on leaks after finalization
    UPCXX_ASSERT_ALWAYS(!is_active(), "An active upcxx::cuda_device must have destroy() called before destructor.");
  }
}

void upcxx::cuda_device::destroy(upcxx::entry_barrier eb) {
  UPCXX_ASSERT_INIT();
  UPCXX_ASSERT_ALWAYS_MASTER();
  UPCXX_ASSERT_COLLECTIVE_SAFE(eb);

  backend::quiesce(upcxx::world(), eb);

  if (!is_active()) return;

  #if UPCXX_CUDA_ENABLED
    cuda::device_state *st = cuda::device_state::get(heap_idx_);
    UPCXX_ASSERT(st != nullptr);
    UPCXX_ASSERT(st->device_id == device_);
    UPCXX_ASSERT(st->ep_index == (gex_EP_Index_t)heap_idx_);

    if (st->alloc_base) {
      detail::device_allocator_core<upcxx::cuda_device>* alloc = 
        static_cast<detail::device_allocator_core<upcxx::cuda_device>*>(st->alloc_base);
      UPCXX_ASSERT(alloc);
      alloc->destroy();
      UPCXX_ASSERT(st->alloc_base == &tombstone);
    }

    // TODO: once they are provided, eventually will do:
    //   gex_Segment_Destroy()
    //   gex_MK_Destroy()
    //   gex_EP_Destroy()  
    // and modify heap_state to allow recycling of heap_idx
    
    CU_CHECK_ALWAYS(cuStreamDestroy(st->stream));
    CU_CHECK_ALWAYS(cuCtxSetCurrent(nullptr));
    CU_CHECK_ALWAYS(cuDevicePrimaryCtxRelease(st->device_id));
    
    backend::heap_state::get(heap_idx_) = nullptr;
    delete st;
  #endif
  
  device_ = invalid_device_id; // deactivate
  heap_idx_ = -1;
}

upcxx::cuda_device::id_type 
upcxx::cuda_device::device_id(detail::internal_only, int heap_idx) {
  #if UPCXX_CUDA_ENABLED
    cuda::device_state *st = cuda::device_state::get(heap_idx);
    int id = st->device_id;
    UPCXX_ASSERT(id != invalid_device_id);
    return id;
  #else
    UPCXX_FATAL_ERROR("Internal error on device_allocator::device_id()");
    return invalid_device_id;
  #endif
}

// non-collective default constructor
detail::device_allocator_core<upcxx::cuda_device>::device_allocator_core():
  detail::device_allocator_base(-1/*inactive*/, segment_allocator(nullptr, 0)) { }

// collective constructor with a (possibly inactive) device
detail::device_allocator_core<upcxx::cuda_device>::device_allocator_core(
    upcxx::cuda_device &dev, void *base, size_t size
  ):
  detail::device_allocator_base(
    dev.heap_idx_,
    #if UPCXX_CUDA_ENABLED
      make_segment(dev.heap_idx_, base, size)
    #else
      segment_allocator(nullptr, 0)
    #endif
  ) {

  #if UPCXX_CUDA_ENABLED
    if (dev.is_active()) {
      backend::heap_state *hs = backend::heap_state::get(dev.heap_idx_);
      UPCXX_ASSERT(hs->alloc_base == this); // registration handled by device_allocator_base
    }
  #endif
}

void detail::device_allocator_core<upcxx::cuda_device>::destroy() {
  if (!is_active()) return;

  #if UPCXX_CUDA_ENABLED  
      cuda::device_state *st = cuda::device_state::get(heap_idx_);
      UPCXX_ASSERT(st);
     
      if(st->segment_to_free) {
        CU_CHECK_ALWAYS(cuCtxPushCurrent(st->context));
        CU_CHECK_ALWAYS(cuMemFree(st->segment_to_free));
        st->segment_to_free = reinterpret_cast<CUdeviceptr>(nullptr);
        CUcontext dump;
        CU_CHECK_ALWAYS(cuCtxPopCurrent(&dump));
      }
      
      st->alloc_base = &tombstone; // deregister
  #endif

  heap_idx_ = -1; // deactivate
}

detail::device_allocator_core<upcxx::cuda_device>::~device_allocator_core() {
  if(upcxx::initialized()) {
    // The thread safety restriction of this call still applies when upcxx isn't
    // initialized, we just have no good way of asserting it so we conditionalize
    // on initialized().
    UPCXX_ASSERT_ALWAYS_MASTER();
  }

  destroy();
}
