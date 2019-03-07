#ifndef _62341dee_845f_407c_9241_cd36da9f0e1c
#define _62341dee_845f_407c_9241_cd36da9f0e1c

#include <upcxx/backend_fwd.hpp>
#include <upcxx/cuda_fwd.hpp>
#include <upcxx/device_allocator.hpp>
#include <upcxx/global_ptr.hpp>
#include <upcxx/memory_kind.hpp>

#include <cstdint>

namespace upcxx {
  namespace cuda {
  #if UPCXX_CUDA_ENABLED
    constexpr int max_devices = 32;
    
    struct device_state; // implemented in cuda_internal.hpp
    
    extern device_state *devices[max_devices];
  #endif
  }

  class cuda_device {
    friend struct detail::device_allocator_core<cuda_device>;
    int device_;
    
  public:
    template<typename T>
    using pointer = T*;

    template<typename T>
    static constexpr T* null_pointer() { return nullptr; }
    
    static constexpr memory_kind kind = memory_kind::cuda_device;

    static constexpr int inactive_device_id = -1;

    cuda_device(int device = inactive_device_id);
    cuda_device(cuda_device const&) = delete;
    cuda_device(cuda_device&&) = default;
    ~cuda_device();

    int device_id() const { return device_; }
    bool is_active() const { return device_ != inactive_device_id; }

    template<typename T>
    static int device_id(global_ptr<T,memory_kind::cuda_device> gp) {
      return gp.device_;
    }
    
    void destroy(upcxx::entry_barrier eb = entry_barrier::user);
  };

  namespace detail {
    template<>
    struct device_allocator_core<cuda_device>: device_allocator_base {
      static constexpr std::size_t min_alignment = 16;

      template<typename T>
      static constexpr std::size_t default_alignment() {
        return alignof(T) < 256 ? 256 : alignof(T);
      }
      
      device_allocator_core(cuda_device *dev, void *base, std::size_t size);
      device_allocator_core(device_allocator_core&&) = default;
      ~device_allocator_core();
    };
  }
}
#endif
