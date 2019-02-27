#include <upcxx/upcxx.hpp>
#include "util.hpp"

#if UPCXX_CUDA_ENABLED
  #include <cuda_runtime_api.h>
  #include <cuda.h>
  constexpr int dev_n = 1; // set to num GPU/process
#else
  constexpr int dev_n = 1;
#endif

using namespace upcxx;

template<typename T>
using any_ptr = global_ptr<T, memory_kind::any>;

int main() {
  upcxx::init();
  print_test_header();
  {
    int me = upcxx::rank_me();
    UPCXX_ASSERT_ALWAYS(upcxx::rank_n() == 2, "Set ranks=2 please.");
    
    std::array<any_ptr<int>,dev_n> buf[2];

    #if UPCXX_CUDA_ENABLED
      cuda_device* gpu[dev_n];
      device_allocator<cuda_device>* seg[dev_n];
      for(int dev=0; dev < dev_n; dev++) {
        gpu[dev] = new cuda_device(dev);
        seg[dev] = new device_allocator<cuda_device>(*gpu[dev], 32<<20);
        buf[me][dev] = seg[dev]->allocate<int>(1<<20);

        int *tmp = new int[1<<20];
        for(int i=0; i < 1<<20; i++)
          tmp[i] = i + me*1000;
        cudaSetDevice(dev);
        cuMemcpyHtoD(reinterpret_cast<CUdeviceptr>(buf[me][dev].raw_ptr_), tmp, sizeof(int)<<20);
        delete[] tmp;
      }
    #else
      buf[me][0] = upcxx::new_array<int>(1<<20);
      for(int i=0; i < 1<<20; i++)
        buf[me][0].local()[i] = i + me;
    #endif

    upcxx::dist_object<std::array<any_ptr<int>,dev_n>> dbuf(buf[me]);

    buf[1-me] = dbuf.fetch(1-me).wait();

    if(me == 0) {
      future<> all = upcxx::make_future();
      for(int i=0; i < 8; i++) {
        for(int dev=0; dev < dev_n; dev++) {
          all = upcxx::when_all(all,
            upcxx::copy(buf[0][dev], buf[1][(dev+1)%dev_n], 1<<20)
            .then([&,i,dev]() {
              return upcxx::copy(buf[1][(dev+1)%dev_n], buf[0][dev], 1<<20)
                .then([=]() { upcxx::say() << "done i="<<i<<" dev="<<dev; });
            })
          );
        }
      }
      all.wait();
    }
    
    upcxx::barrier();

    #if UPCXX_CUDA_ENABLED
      int *tmp = new int[1<<20];
      for(int dev=0; dev < dev_n; dev++) {
        cudaSetDevice(dev);
        cuMemcpyDtoH(tmp, reinterpret_cast<CUdeviceptr>(buf[me][dev].raw_ptr_), sizeof(int)<<20);
        for(int i=0; i < 1<<20; i++)
          UPCXX_ASSERT_ALWAYS(tmp[i] == i + 0*1000, "Wanted "<<i<<" got "<<tmp[i]);
      }
      delete[] tmp;
    #else
      for(int i=0; i < 1<<20; i++)
        UPCXX_ASSERT_ALWAYS(buf[me][0].local()[i] == i + 0*1000);
    #endif
    
    upcxx::barrier();

    #if UPCXX_CUDA_ENABLED
      for(int i=0; i < dev_n; i++) {
        seg[i]->deallocate(upcxx::static_kind_cast<memory_kind::cuda_device>(buf[me][i]));
        delete seg[i];
        gpu[i]->destroy();
        delete gpu[i];
      }
    #else
      upcxx::delete_array(upcxx::static_kind_cast<memory_kind::host>(buf[me][0]));
    #endif
  }
  print_test_success();
  upcxx::finalize();
  return 0;
}
