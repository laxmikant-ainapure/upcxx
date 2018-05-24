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
  
  // The receiver-side rpc message type. Inherits lpc base type since rpc's
  // reside in the lpc queues.
  struct rpc_as_lpc: detail::lpc_base {
    detail::lpc_vtable the_vtbl;
    void *payload; // serialized rpc command
    bool is_rdzv; // was this shipped via rdzv?
    intrank_t rank_s; // only used when shipped via rdzv
    
    // rpc producer's should use `payload_of` and `cleanup` as the similarly
    // named template parameters to `command<lpc_base*>::pack()`. That will allow
    // the `executor` function of the command to be used as the `execute_and_delete`
    // of the lpc.
    static void* payload_of(detail::lpc_base *me) {
      return static_cast<rpc_as_lpc*>(me)->payload;
    }
    
    template<bool never_rdzv>
    static void cleanup(detail::lpc_base *me1);
    
    // Build copy of a packed command buffer (upcxx/command.hpp) as a rpc_as_lpc.
    static rpc_as_lpc* build_copy(
      void *cmd_buf,
      std::size_t cmd_size,
      std::size_t cmd_alignment // alignment requirement of packing
    );
    
    // Allocate space to receive a rdzv. The payload should be used as the dest
    // of the GET. When the GET completes, the command executor should be copied
    // into our `execute_and_delete`.
    static rpc_as_lpc* build_rdzv_lz(
      intrank_t rank_s,
      std::size_t cmd_size,
      std::size_t cmd_alignment // alignment requirement of packing
    );
  };
  
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
          {/*Ignoring return value, casting to (void) is not enough!*/}
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
    detail::persona_tls &tls = detail::the_persona_tls;
    
    UPCXX_ASSERT(!UPCXX_BACKEND_GASNET_SEQ || backend::master.active_with_caller(tls));
    
    persona &p = UPCXX_BACKEND_GASNET_SEQ
      ? backend::master
      : *tls.get_top_persona();
    
    tls.during(
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
  
  template<upcxx::progress_level level, typename Fn1>
  void send_am_master(intrank_t recipient, Fn1 &&fn) {
    UPCXX_ASSERT(!UPCXX_BACKEND_GASNET_SEQ || backend::master.active_with_caller());
    
    using gasnet::eager_buffer;
    using gasnet::rpc_as_lpc;
    
    using Fn = typename std::decay<Fn1>::type;
    
    auto ub = command<detail::lpc_base*>::ubound(parcel_size_empty(), fn);
    
    constexpr bool definitely_eager = ub.static_size <= gasnet::am_size_rdzv_cutover_min;
    bool eager = ub.size <= gasnet::am_size_rdzv_cutover_min &&
                 ub.size <= gasnet::am_size_rdzv_cutover;

    eager_buffer<decltype(ub)> ebuf;
    void *buf;
    
    if(eager)
      buf = ebuf.allocate(ub);
    else
      buf = upcxx::allocate(ub.size, ub.align);
    
    parcel_writer w(buf);
    command<detail::lpc_base*>::template pack<
        rpc_as_lpc::payload_of,
        rpc_as_lpc::template cleanup</*never_rdzv=*/definitely_eager>
      >(w, ub.size, fn);

    if(eager)
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
    
    using gasnet::eager_buffer;
    using gasnet::rpc_as_lpc;
    
    auto ub = command<detail::lpc_base*>::ubound(parcel_size_empty(), fn);
    
    constexpr bool definitely_eager = ub.static_size <= gasnet::am_size_rdzv_cutover_min;
    bool eager = ub.size <= gasnet::am_size_rdzv_cutover_min ||
                 ub.size <= gasnet::am_size_rdzv_cutover;

    gasnet::eager_buffer<decltype(ub)> ebuf;
    void *buf;
    
    if(eager)
      buf = ebuf.allocate(ub);
    else
      buf = upcxx::allocate(ub.size, ub.align);
    
    parcel_writer w(buf);
    command<detail::lpc_base*>::template pack<
        rpc_as_lpc::payload_of,
        rpc_as_lpc::template cleanup</*never_rdzv=*/definitely_eager>
      >(w, ub.size, fn);
    
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
  
  inline handle_cb_queue& get_handle_cb_queue() {
    UPCXX_ASSERT(!UPCXX_BACKEND_GASNET_SEQ || backend::master.active_with_caller());

    #if UPCXX_BACKEND_GASNET_SEQ
      return gasnet::master_hcbs;
    #elif UPCXX_BACKEND_GASNET_PAR
      return upcxx::current_persona().backend_state_.hcbs;
    #endif
  }
  
  inline void register_cb(handle_cb *cb) {
    get_handle_cb_queue().enqueue(cb);
  }
  
  //////////////////////////////////////////////////////////////////////
  // send_am_restricted
  
  inline void* restricted_payload_of(void *p) { return p; }
  inline void restricted_cleanup(void *p) {}
  
  template<typename Fn>
  void send_am_restricted(intrank_t recipient, Fn &&fn) {
    UPCXX_ASSERT(!UPCXX_BACKEND_GASNET_SEQ || backend::master.active_with_caller());
    
    auto ub = command<detail::lpc_base*>::ubound(parcel_size_empty(), fn);
    eager_buffer<decltype(ub)> ebuf;
    void *buf = ebuf.allocate(ub);
    
    parcel_writer w(buf);
    command<void*>::pack<
        restricted_payload_of,
        restricted_cleanup
      >(w, ub.size, fn);
    
    gasnet::send_am_eager_restricted(recipient, buf, w.size(), w.align());
  }
  
  //////////////////////////////////////////////////////////////////////
  // rpc_as_lpc
  
  template<>
  inline void rpc_as_lpc::cleanup</*never_rdzv=*/true>(detail::lpc_base *me1) {
    rpc_as_lpc *me = static_cast<rpc_as_lpc*>(me1);
    std::free(me->payload);
  }
}}}

#endif
