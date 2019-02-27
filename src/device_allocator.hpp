#ifndef _0a792abf_0420_42b8_91a0_67a4b337f136
#define _0a792abf_0420_42b8_91a0_67a4b337f136

#include <upcxx/cuda.hpp>
#include <upcxx/global_ptr.hpp>
#include <upcxx/segment_allocator.hpp>

namespace upcxx {
  namespace detail {
    struct device_allocator_base {
      int device_;
      detail::segment_allocator seg_;

    public:
      device_allocator_base(int device, detail::segment_allocator seg):
        device_(device),
        seg_(std::move(seg)) {
      }
      device_allocator_base(device_allocator_base const&) = delete;
      device_allocator_base(device_allocator_base&&) = default;
    };

    // specialized per device type
    template<typename Device>
    struct device_allocator_core; /*: device_allocator_base {
      static constexpr std::size_t default_alignment = ???;
      static constexpr std::size_t min_alignment = ???;
      device_allocator_core(Device &dev, typename Device::pointer<void> base, std::size_t size);
      device_allocator_core(device_allocator_core&&) = default;
    };*/
  }
  
  template<typename Device>
  class device_allocator: detail::device_allocator_core<Device> {
  public:
    using device_type = Device;
    
    device_allocator(Device &dev, typename Device::template pointer<void> base, std::size_t size):
      detail::device_allocator_core<Device>(dev, base, size) {
    }
    device_allocator(Device &dev, std::size_t size):
      detail::device_allocator_core<Device>(dev, Device::template null_pointer<void>(), size) {
    }
    
    device_allocator(device_allocator&&) = default;

    template<typename T,
             std::size_t align = alignof(T) < detail::device_allocator_core<Device>::default_alignment
                               ? detail::device_allocator_core<Device>::default_alignment
                               : alignof(T)>
    global_ptr<T,Device::kind> allocate(std::size_t n=1) {
      void *ptr = this->seg_.allocate(
          n*sizeof(T),
          std::min<std::size_t>(align, this->min_alignment)
        );
      
      return ptr == nullptr
        ? global_ptr<T,Device::kind>(nullptr)
        : global_ptr<T,Device::kind>(
          detail::internal_only(),
          upcxx::rank_me(),
          (T*)ptr,
          this->device_
        );
    }

    template<typename T>
    void deallocate(global_ptr<T,Device::kind> p) {
      if(p) {
        UPCXX_ASSERT(p.device_ == this->device_ && p.rank_ == upcxx::rank_me());
        this->seg_.deallocate(p.raw_ptr_);
      }
    }

    template<typename T>
    global_ptr<T,Device::kind> to_global_ptr(typename Device::template pointer<T> p) const {
      return global_ptr<T,Device::kind>(
        detail::internal_only(),
        upcxx::rank_me(),
        p,
        this->device_
      );
    }

    template<typename T>
    global_ptr<T,Device::kind> try_global_ptr(typename Device::template pointer<T> p) const {
      return this->seg_.in_segment((void*)p)
        ? global_ptr<T,Device::kind>(
          detail::internal_only(),
          upcxx::rank_me(),
          p,
          this->device_
        )
        : global_ptr<T,Device::kind>(nullptr);
    }

    template<typename T, memory_kind K>
    typename Device::template pointer<T> raw_pointer(global_ptr<T,K> gp) const {
      UPCXX_ASSERT(gp.is_null() || gp.dynamic_kind() == Device::kind);
      return gp.raw_ptr_;
    }
  };
}

#endif
