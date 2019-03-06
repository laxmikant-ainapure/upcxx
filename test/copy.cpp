#include <upcxx/upcxx.hpp>
#include "util.hpp"

#if UPCXX_CUDA_ENABLED
  #include <cuda_runtime_api.h>
  #include <cuda.h>
  constexpr int max_dev_n = 32;
  int dev_n;
#else
  constexpr int max_dev_n = 0; // set to num GPU/process
  constexpr int dev_n = 0;
#endif

constexpr int rounds = 4;
    
using namespace upcxx;

template<typename T>
using any_ptr = global_ptr<T, memory_kind::any>;

int main() {
  upcxx::init();
  print_test_header();
  {
    int me = upcxx::rank_me();
    UPCXX_ASSERT_ALWAYS(upcxx::rank_n() >= 2, "Set ranks>=2 please.");

    #if UPCXX_CUDA_ENABLED
      cuInit(0);
      cuDeviceGetCount(&dev_n);
      if(dev_n >= max_dev_n)
        dev_n = max_dev_n-1;
    #endif

    if(me == 0) {
      std::cerr<<"Running with devices="<<dev_n<<'\n';
    }
    
    // buf[rank][1+dev][buf]
    std::array<std::array<any_ptr<int>,2>,1+max_dev_n> buf[2];

    if(me < 2) {
      buf[me][0][0] = upcxx::new_array<int>(1<<20);
      buf[me][0][1] = upcxx::new_array<int>(1<<20);
      for(int i=0; i < 1<<20; i++)
        buf[me][0][0].local()[i] = (i%(1<<17)%10) + (i>>17)*10 + (0*100) + (me*1000);
    }

    #if UPCXX_CUDA_ENABLED
      cuda_device* gpu[dev_n];
      device_allocator<cuda_device>* seg[dev_n];
      for(int dev=1; dev < 1+dev_n; dev++) {
        gpu[dev-1] = new cuda_device(dev-1);
        seg[dev-1] = new device_allocator<cuda_device>(*gpu[dev-1], 32<<20);

        buf[me][dev][0] = seg[dev-1]->allocate<int>(1<<20);
        buf[me][dev][1] = seg[dev-1]->allocate<int>(1<<20);

        if(me < 2) {
          int *tmp = new int[1<<20];
          for(int i=0; i < 1<<20; i++)
            tmp[i] = (i%(1<<17)%10) + (i>>17)*10 + (dev*100) + (me*1000);
          cudaSetDevice(dev-1);
          cuMemcpyHtoD(reinterpret_cast<CUdeviceptr>(buf[me][dev][0].raw_ptr_), tmp, sizeof(int)<<20);
          delete[] tmp;
        }
      }
    #endif
    
    upcxx::broadcast(&buf[0], 1, 0).wait();
    upcxx::broadcast(&buf[1], 1, 1).wait();
    
    for(int initiator=0; initiator < upcxx::rank_n(); initiator++) {
      if(me == initiator) {
        for(int round=0; round < rounds; round++) {
          future<> all = upcxx::make_future();
          for(int dr=0; dr < 2; dr++) { // dest rank loop
            for(int dd=0; dd < 1+dev_n; dd++) { // dest dev loop
              for(int dp=0; dp < 8; dp++) { // dest part loop
                // compute source rank,dev,part using overflowing increment per round
                int sp = dp, sd = dd, sr = dr;
                for(int r=0;  r < round+1; r++) {
                  sp = (sp + 1) % 8;
                  sd = (sd + (sp == 0 ? 1 : 0)) % (1+dev_n);
                  sr = (sr + (sd == 0 ? 1 : 0)) % 2;
                }

                auto src = buf[sr][sd][(round+0)%2] + (sp<<17);
                auto dst = buf[dr][dd][(round+1)%2] + (dp<<17);
                all = upcxx::when_all(all, upcxx::copy(src, dst, 1<<17));
              }
            }
          }
          all.wait();
          std::cerr<<"done round="<<round<<" initiator="<<initiator<<'\n';
        }
      }
      
      upcxx::barrier();
    }

    if(me < 2) {
      for(int dd=0; dd < 1+dev_n; dd++) { // dev loop
        for(int dp=0; dp < 8; dp++) { // part loop
          int sp = dp, sd = dd, sr = me;
          // compute source part,dev,rank for all rounds over all initiators
          for(int initiator=0; initiator < upcxx::rank_n(); initiator++) {
            for(int round=0; round < rounds; round++) {
              for(int r=0;  r < round+1; r++) {
                sp = (sp + 1) % 8;
                sd = (sd + (sp == 0 ? 1 : 0)) % (1+dev_n);
                sr = (sr + (sd == 0 ? 1 : 0)) % 2;
              }
            }
          }

          int *tmp;
          if(dd == 0)
            tmp = buf[me][dd][rounds%2].local() + (dp<<17);
          else {
          #if UPCXX_CUDA_ENABLED
            tmp = new int[1<<17];
            cudaSetDevice(dd-1);
            cuMemcpyDtoH(tmp, reinterpret_cast<CUdeviceptr>(buf[me][dd][rounds%2].raw_ptr_ + (dp<<17)), sizeof(int)<<17);
          #endif
          }
          
          for(int i=0; i < 1<<17; i++) {
            int expect = (i%10) + (sp*10) + (sd*100) + (sr*1000);
            UPCXX_ASSERT_ALWAYS(tmp[i] == expect, "Expected "<<expect<<" got "<<tmp[i]);
          }

          if(dd != 0)
            delete[] tmp;
        }
      }
    }
    
    upcxx::barrier();

    if(me < 2) {
      upcxx::delete_array(upcxx::static_kind_cast<memory_kind::host>(buf[me][0][0]));
      upcxx::delete_array(upcxx::static_kind_cast<memory_kind::host>(buf[me][0][1]));
    }
    
    #if UPCXX_CUDA_ENABLED
      for(int dev=1; dev < 1+dev_n; dev++) {
        if(me < 2) {
          seg[dev-1]->deallocate(upcxx::static_kind_cast<memory_kind::cuda_device>(buf[me][dev][0]));
          seg[dev-1]->deallocate(upcxx::static_kind_cast<memory_kind::cuda_device>(buf[me][dev][1]));
        }
        delete seg[dev-1];
        gpu[dev-1]->destroy();
        delete gpu[dev-1];
      }
    #endif
  }
    
  print_test_success();
  upcxx::finalize();
  return 0;
}
