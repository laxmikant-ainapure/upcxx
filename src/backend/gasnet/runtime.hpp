#ifndef _223a1448_cf6d_42a9_864f_57409c60efe9
#define _223a1448_cf6d_42a9_864f_57409c60efe9

#include <upcxx/backend.hpp>
#include <upcxx/bind.hpp>
#include <upcxx/command.hpp>

#include <upcxx/backend/gasnet/handle_cb.hpp>

#include <cstdint>
#include <cstdlib>

////////////////////////////////////////////////////////////////////////

#if !NOBS_DISCOVERY && !UPCXX_BACKEND_GASNET
  #error "This header can only be used when the GASNet backend is enabled."
#endif

////////////////////////////////////////////////////////////////////////
// declarations for: upcxx/backend/gasnet/runtime.cpp

namespace upcxx {
namespace backend {
namespace gasnet {
  static constexpr std::size_t am_size_rdzv_cutover_min = 256;
  extern std::size_t am_size_rdzv_cutover;

  #if UPCXX_BACKEND_GASNET_SEQ
    extern handle_cb_queue master_hcbs;
  #endif
  
  void after_gasnet();

  // Register a handle callback for the current persona
  void register_cb(handle_cb *cb);
  
  // Send AM (packed command), receiver executes in handler.
  void send_am_eager_restricted(
    intrank_t recipient,
    void *command_buf,
    std::size_t buf_size,
    std::size_t buf_align
  );
  
  // Send fully bound callable, receiver executes in handler.
  template<typename Fn>
  void send_am_restricted(intrank_t recipient, Fn &&fn);
  
  // Send AM (packed command), receiver executes in `level` progress.
  void send_am_eager_master(
    progress_level level,
    intrank_t recipient,
    void *command_buf,
    std::size_t buf_size,
    std::size_t buf_align
  );
  void send_am_eager_persona(
    progress_level level,
    intrank_t recipient_rank,
    persona *recipient_persona,
    void *command_buf,
    std::size_t buf_size,
    std::size_t buf_align
  );
  
  // Send AM (packed command) via rendezvous, receiver executes druing `level`.
  template<progress_level level>
  void send_am_rdzv(
    intrank_t recipient_rank,
    persona *recipient_persona, // nullptr == master
    void *command_buf,
    std::size_t buf_size, std::size_t buf_align
  );
}}}

//////////////////////////////////////////////////////////////////////
// implementation of: upcxx::backend

namespace upcxx {
namespace backend {
  //////////////////////////////////////////////////////////////////////
  // during_level
  
  template<typename Fn>
  void during_level(std::integral_constant<progress_level, progress_level::internal>, Fn &&fn) {
    UPCXX_ASSERT(!UPCXX_BACKEND_GASNET_SEQ || backend::master.active_with_caller());
    fn();
  }
  template<typename Fn>
  void during_level(std::integral_constant<progress_level, progress_level::user>, Fn &&fn) {
    UPCXX_ASSERT(!UPCXX_BACKEND_GASNET_SEQ || backend::master.active_with_caller());
    
    persona &p = UPCXX_BACKEND_GASNET_SEQ
      ? backend::master
      : *detail::tl_top_persona;
    
    detail::persona_during(
      p, progress_level::user, std::forward<Fn>(fn),
      /*known_active=*/std::true_type{}
    );
  }

  template<progress_level level, typename Fn>
  void during_level(Fn &&fn) {
    during_level(std::integral_constant<progress_level,level>{}, std::forward<Fn>(fn));
  }
  
  //////////////////////////////////////////////////////////////////////
  // send_am_{master|persona}

  template<typename ParcelSize,
           bool is_small = ParcelSize::all_static && (ParcelSize::static_size <= 1024)>
  struct eager_buffer;

  template<typename ParcelSize>
  struct eager_buffer<ParcelSize, /*is_small=*/false> {
    void *big_ = nullptr;
    typename std::aligned_storage<
        512,
        ParcelSize::is_align_static ? ParcelSize::static_align : 64
      >::type tiny_;
    
    void* allocate(ParcelSize ub) {
      if(ub.size <= 512 && ub.align <= 64) {
        big_ = nullptr;
        return &tiny_;
      }
      else {
        if(posix_memalign(&big_, ub.align, ub.size))
          {/* ignoring return value */}
        return big_;
      }
    }
    
    ~eager_buffer() {
      if(big_ != nullptr)
        std::free(big_);
    }
  };

  template<typename ParcelSize>
  struct eager_buffer<ParcelSize, /*is_small=*/true> {
    typename std::aligned_storage<ParcelSize::size, ParcelSize::align>::type buf_;

    void* allocate(ParcelSize) {
      return &buf_;
    }
  };

  inline void am_cleanup(bool use_free, void *buf) {
    if(use_free)
      std::free(buf);
    else
      upcxx::deallocate(buf);
  };
  
  template<upcxx::progress_level level, typename Fn1>
  void send_am_master(intrank_t recipient, Fn1 &&fn) {
    UPCXX_ASSERT(!UPCXX_BACKEND_GASNET_SEQ || backend::master.active_with_caller());

    using Fn = typename std::decay<Fn1>::type;
    
    auto ub = command<bool,void*>::ubound(parcel_size_empty(), fn);
    
    bool rdzv = ub.size > gasnet::am_size_rdzv_cutover_min &&
                ub.size > gasnet::am_size_rdzv_cutover;

    eager_buffer<decltype(ub)> ebuf;
    void *buf;
    
    if(!rdzv)
      buf = ebuf.allocate(ub);
    else
      buf = upcxx::allocate(ub.size, ub.align);

    parcel_writer w{buf};
    command<bool,void*>::pack<am_cleanup>(w, ub.size, fn);

    if(!rdzv)
      gasnet::send_am_eager_master(level, recipient, buf, w.size(), w.align());
    else
      gasnet::template send_am_rdzv<level>(recipient, /*master*/nullptr, buf, w.size(), w.align());
  }
  
  template<upcxx::progress_level level, typename Fn>
  void send_am_persona(
      intrank_t recipient_rank,
      persona *recipient_persona,
      Fn &&fn
    ) {
    UPCXX_ASSERT(!UPCXX_BACKEND_GASNET_SEQ || backend::master.active_with_caller());
    
    auto ub = command<bool,void*>::ubound(parcel_size_empty(), fn);
    
    bool eager = ub.size <= gasnet::am_size_rdzv_cutover_min ||
                 ub.size <= gasnet::am_size_rdzv_cutover;

    eager_buffer<decltype(ub)> ebuf;
    void *buf;
    
    if(eager)
      buf = ebuf.allocate(ub);
    else
      buf = upcxx::allocate(ub.size, ub.align);
    
    parcel_writer w{buf};
    command<bool,void*>::pack<am_cleanup>(w, ub.size, fn);
    
    if(eager)
      gasnet::send_am_eager_persona(level, recipient_rank, recipient_persona, buf, w.size(), w.align());
    else
      gasnet::send_am_rdzv<level>(recipient_rank, recipient_persona, buf, w.size(), w.align());
  }
}}

//////////////////////////////////////////////////////////////////////
// implementation of: upcxx::backend::gasnet

namespace upcxx {
namespace backend {
namespace gasnet {
  //////////////////////////////////////////////////////////////////////
  // register_handle_cb
  
  inline void register_cb(handle_cb *cb) {
    UPCXX_ASSERT(!UPCXX_BACKEND_GASNET_SEQ || backend::master.active_with_caller());

    #if UPCXX_BACKEND_GASNET_SEQ
      gasnet::master_hcbs.enqueue(cb);
    #elif UPCXX_BACKEND_GASNET_PAR
      upcxx::current_persona().backend_state_.hcbs.enqueue(cb);
    #endif
  }
  
  //////////////////////////////////////////////////////////////////////
  // send_am_restricted
  
  template<typename Fn>
  void send_am_restricted(intrank_t recipient, Fn &&fn) {
    UPCXX_ASSERT(!UPCXX_BACKEND_GASNET_SEQ || backend::master.active_with_caller());
    
    auto ub = command<>::ubound(parcel_size_empty(), fn);
    
    bool rdzv = ub.size > gasnet::am_size_rdzv_cutover_min &&
                ub.size > gasnet::am_size_rdzv_cutover;

    eager_buffer<decltype(ub)> ebuf;
    void *buf;
    
    if(!rdzv)
      buf = ebuf.allocate(ub);
    else
      buf = upcxx::allocate(ub.size, ub.align);
    
    parcel_writer w{buf};
    command<>::pack(w, ub.size, fn);
    
    if(!rdzv)
      gasnet::send_am_eager_restricted(recipient, buf, w.size(), w.align());
    else {
      gasnet::send_am_rdzv<progress_level::internal>(
        recipient, /*master*/nullptr, buf, w.size(), w.align()
      );
    }
  }
}}}

#endif
