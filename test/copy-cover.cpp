#include <string>

#include <upcxx/upcxx.hpp>
#include "util.hpp"

#ifndef USE_HOST
#define USE_HOST 1
#endif

#ifndef USE_CUDA
  #if UPCXX_CUDA_ENABLED 
    #define USE_CUDA 1
  #else
    #define USE_CUDA 0
  #endif
#endif
#if USE_CUDA && !UPCXX_CUDA_ENABLED
  #error requested USE_CUDA but this UPC++ install does not have CUDA support
#endif


#if USE_CUDA
  #include <cuda_runtime_api.h>
  #include <cuda.h>
  int dev_n;
  constexpr int max_dev_n = 2;
#else
  constexpr int dev_n = 0;
#endif

using namespace upcxx;

using val_t = std::uint32_t;
#define VAL(rank, step, idx) ((val_t)(((rank)&0xFFFF << 16) | ((step)&0xFF << 8) | ((idx)&0xFF) ))

using any_ptr = global_ptr<val_t, memory_kind::any>;

int main(int argc, char *argv[]) {
  upcxx::init();
  print_test_header();

  int me = upcxx::rank_me();
  int ranks = upcxx::rank_n();
  int peer1 = (upcxx::rank_me()+1)%ranks;
  int peer2 = (upcxx::rank_me()+1)%ranks;
  long iters = 0;
  if (argc > 1) iters = std::atol(argv[1]);
  if (iters <= 0) iters = 10;
  size_t bufelems;
  { size_t bufsz = 0;
    if (argc > 2) bufsz = std::atol(argv[2]);
    if (bufsz <= 0) bufsz = 1024*1024;
    if (bufsz < sizeof(val_t)) bufsz = sizeof(val_t);
    bufelems = bufsz / sizeof(val_t);
    if (!me) say("") << "Running with iters=" << iters << " bufsz=" << bufelems*sizeof(val_t) << " bytes"; 
  }

  {
    if(me == 0 && ranks < 3)
      say("") << "Advice: consider using 3 (or more) ranks to cover three-party cases for upcxx::copy.";

    #if USE_CUDA
    {
      cuInit(0);
      cuDeviceGetCount(&dev_n);

      int lo = upcxx::reduce_all(dev_n, upcxx::op_fast_min).wait();
      int hi = upcxx::reduce_all(dev_n, upcxx::op_fast_max).wait();

      if(me == 0 && lo != hi)
        say("")<<"Notice: not all ranks report the same number of GPUs: min="<<lo<<" max="<<hi;

      if (!lo) {
        if (!me) say("")<<"WARNING: UPC++ CUDA support is compiled-in, but could not find sufficient GPU support at runtime.";
        dev_n = lo;
      }
    }
    #endif

    say()<<"Running with devices="<<dev_n;
   
    std::vector<any_ptr> ptrs;
    constexpr int allocs_per_heap = 3;
    // fill ptrs with global ptrs to buffers, with allocs_per_heap

    global_ptr<val_t> host_ptrs[allocs_per_heap];
    for (int i=0; i < allocs_per_heap; i++) {
    #if USE_HOST
      host_ptrs[i] = upcxx::new_array<val_t>(bufelems*2);
      int rank = (me+i)%ranks;
      dist_object<any_ptr> dobj(host_ptrs[i]);
      any_ptr gp = dobj.fetch(rank).wait();
      ptrs.push_back(gp);
      barrier();
    #else
      host_ptrs[i] = nullptr;
    #endif
    }

    #if USE_CUDA
      cuda_device* gpu[max_dev_n] = {};
      device_allocator<cuda_device>* seg[max_dev_n] = {};
      global_ptr<val_t, memory_kind::cuda_device> cuda_ptrs[max_dev_n][allocs_per_heap] = {};

    if (dev_n) {
      for (int dev = 0; dev < max_dev_n; dev++) {
        size_t align = cuda_device::default_alignment<val_t>();
        size_t allocsz = bufelems*2*sizeof(val_t);
        allocsz = align*((allocsz+align-1)/align);
        align = 4096;
        if (allocsz > align) { // more than one page gets a full page
          allocsz = align*((allocsz+align-1)/align);
        }
        gpu[dev] = new cuda_device(dev%dev_n);
        seg[dev] = new device_allocator<cuda_device>(*gpu[dev], allocsz*allocs_per_heap);
        for (int i=0; i < allocs_per_heap; i++) {
          cuda_ptrs[dev][i] = seg[dev]->allocate<val_t>(bufelems*2);
          assert(cuda_ptrs[dev][i]);
          int rank = (me+i)%ranks;
          dist_object<any_ptr> dobj(cuda_ptrs[dev][i]);
          any_ptr gp = dobj.fetch(rank).wait();
          ptrs.push_back(gp);
          barrier();
        }
      }
    }
    #endif

    val_t *priv_src = new val_t[bufelems];
    val_t *priv_dst = new val_t[bufelems];
    const int bufcnt = ptrs.size();


    uint64_t step = 0;
    static uint64_t rc_count = 0;
    for (int round = 0; round < iters; round++) {
      bool talk = !me && (iters <= 10 || round % ((iters+9)/10) == 0);
      if (talk) {
        say("") << "Round "<< round << " (" << round*100/iters << " %)";
      }
      upcxx::barrier();
      
      for (int A=0; A < bufcnt; A++) {
      for (int B=0; B < bufcnt; B++) {
        any_ptr bufA = ptrs[A];
        any_ptr bufB = ptrs[B] + bufelems;

        for(int i=0; i < bufelems; i++) {
          priv_src[i] = VAL(me, step, i);
          priv_dst[i] = 0;
        }

        #if SKIP_COPY // for tester validation
          memcpy(priv_dst, priv_src, bufelems*sizeof(val_t));
        #else
          auto cxs = operation_cx::as_future() | source_cx::as_future();
          auto rc = [](int rank) { 
            UPCXX_ASSERT_ALWAYS(&upcxx::current_persona() == &upcxx::master_persona());
            UPCXX_ASSERT_ALWAYS(rank == upcxx::rank_me());
            rc_count++;
          };
          future<> of, sf;

          // private -> heapA
          std::tie(of, sf) = upcxx::copy<val_t>(priv_src, bufA, bufelems, 
                                                cxs | remote_cx::as_rpc(rc,bufA.where()));
          sf.wait(); 
          of.wait();

          // heapA -> heapB
          std::tie(of, sf) = upcxx::copy<val_t>(bufA, bufB, bufelems,
                                                cxs | remote_cx::as_rpc(rc,bufB.where()));
          sf.wait(); 
          of.wait();

          // heapB -> private
          std::tie(of, sf) = upcxx::copy<val_t>(bufB, priv_dst, bufelems,
                                                cxs | remote_cx::as_rpc(rc,me));
          sf.wait(); 
          of.wait();

        #endif

        std::string mismatch;
        for(int i=0; i < bufelems; i++) {
          val_t got = priv_dst[i];
          val_t expect = VAL(me, step, i);
          if (got != expect && !mismatch.size()) {
            std::ostringstream oss;
            oss << " i=" << i << " expect=" << expect << " got=" << got;
            mismatch = oss.str();
          }
        }
        if (mismatch.size()) {
          say() << "ERROR: Mismatch at round="<<round<<" step="<<step<<" A="<<A<<" B="<<B<<mismatch;
        }

        step++;
      }} // A/B bufs

      do { upcxx::progress(); } while (rc_count < 3 * bufcnt*bufcnt);
      rc_count = 0;
      upcxx::barrier();
    } // round
    
    upcxx::barrier();

    // cleanup

    delete [] priv_src;
    delete [] priv_dst;

    for (int i=0; i < allocs_per_heap; i++) {
      upcxx::delete_array(host_ptrs[i]);
    }
    
    #if USE_CUDA
    if (dev_n) {
      for (int dev = 0; dev < max_dev_n; dev++) {
        for (int i=0; i < allocs_per_heap; i++) {
          seg[dev]->deallocate(cuda_ptrs[dev][i]);
        }
        delete seg[dev];
        gpu[dev]->destroy();
        delete gpu[dev];
      }
    }
    #endif
  }
    
  UPCXX_ASSERT_ALWAYS(&upcxx::current_persona() == &upcxx::master_persona());
  print_test_success();
  
  upcxx::finalize();
  return 0;
}
