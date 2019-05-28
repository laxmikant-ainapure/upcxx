#ifndef _223a1448_cf6d_42a9_864f_57409c60efe9
#define _223a1448_cf6d_42a9_864f_57409c60efe9

#include <upcxx/backend/gasnet/runtime_fwd.hpp>
#include <upcxx/backend/gasnet/handle_cb.hpp>

#include <upcxx/backend_fwd.hpp>
#include <upcxx/bind.hpp>
#include <upcxx/command.hpp>
#include <upcxx/persona.hpp>
#include <upcxx/team_fwd.hpp>

#include <cstdint>
#include <cstdlib>

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
    team &tm,
    intrank_t recipient,
    void *command_buf,
    std::size_t buf_size,
    std::size_t buf_align
  );
  
  // Send fully bound callable, receiver executes in handler.
  template<typename Fn>
  void send_am_restricted(team &tm, intrank_t recipient, Fn &&fn);
  
  // Send AM (packed command), receiver executes in `level` progress.
  void send_am_eager_master(
    progress_level level,
    team &tm,
    intrank_t recipient,
    void *command_buf,
    std::size_t buf_size,
    std::size_t buf_align
  );
  void send_am_eager_persona(
    progress_level level,
    team &tm,
    intrank_t recipient_rank,
    persona *recipient_persona,
    void *command_buf,
    std::size_t buf_size,
    std::size_t buf_align
  );
  
  // Send AM (packed command) via rendezvous, receiver executes druing `level`.
  template<progress_level level>
  void send_am_rdzv(
    team &tm,
    intrank_t recipient_rank,
    persona *recipient_persona, // nullptr == master
    void *command_buf,
    std::size_t buf_size, std::size_t buf_align
  );
  
  void bcast_am_master_eager(
    progress_level level,
    team &tm,
    intrank_t rank_d_ub, // in range [0, 2*rank_n-1)
    void *payload,
    size_t cmd_size,
    size_t cmd_align
  );
  template<progress_level level>
  int/*refs_added*/ bcast_am_master_rdzv(
    team &tm,
    intrank_t rank_d_ub, // in range [0, 2*rank_n-1)
    intrank_t wrank_owner, // world team coordinates
    void *payload_sender,
    std::atomic<std::int64_t> *refs_sender,
    size_t cmd_size,
    size_t cmd_align
  );
  
  // The receiver-side rpc message type. Inherits lpc base type since rpc's
  // reside in the lpc queues.
  struct rpc_as_lpc: detail::lpc_base {
    detail::lpc_vtable the_vtbl;
    void *payload; // serialized rpc command
    bool is_rdzv; // was this shipped via rdzv?
    bool rdzv_rank_s_local; // only used when shipped via rdzv
    intrank_t rdzv_rank_s; // only used when shipped via rdzv
    
    // rpc producer's should use `reader_of` and `cleanup` as the similarly
    // named template parameters to `command<lpc_base*>::pack()`. That will allow
    // the `executor` function of the command to be used as the `execute_and_delete`
    // of the lpc.
    static parcel_reader reader_of(detail::lpc_base *me) {
      return parcel_reader(static_cast<rpc_as_lpc*>(me)->payload);
    }
    
    template<bool never_rdzv>
    static void cleanup(detail::lpc_base *me);
    
    // Build copy of a packed command buffer (upcxx/command.hpp) as a rpc_as_lpc.
    template<typename RpcAsLpc = rpc_as_lpc>
    static RpcAsLpc* build_eager(
      void *cmd_buf,
      std::size_t cmd_size,
      std::size_t cmd_alignment // alignment requirement of packing
    );
    
    // Allocate space to receive a rdzv. The payload should be used as the dest
    // of the GET. When the GET completes, the command executor should be copied
    // into our `execute_and_delete`.
    template<typename RpcAsLpc = rpc_as_lpc>
    static RpcAsLpc* build_rdzv_lz(
      std::size_t cmd_size,
      std::size_t cmd_alignment // alignment requirement of packing
    );
  };
  
  // The receiver-side bcast'd rpc message type. Inherits lpc base type since rpc's
  // reside in the lpc queues.
  struct bcast_as_lpc: rpc_as_lpc {
    union {
      int eager_refs;
      std::atomic<std::int64_t> *rdzv_refs_s;
    };
    std::atomic<std::int64_t> rdzv_refs_here;
    
    static parcel_reader reader_of(detail::lpc_base *me) {
      parcel_reader r(static_cast<bcast_as_lpc*>(me)->payload);
      r.pop_trivial_aligned<team_id>();
      r.pop_trivial_aligned<intrank_t>();
      return r;
    }
    
    template<bool never_rdzv>
    static void cleanup(detail::lpc_base *me);
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
  void during_level(
      std::integral_constant<progress_level, progress_level::internal>,
      Fn &&fn,
      persona &active_per
    ) {
    
    // TODO: revisit the purpose of this seemingly wrong assertion
    //UPCXX_ASSERT(!UPCXX_BACKEND_GASNET_SEQ || backend::master.active_with_caller());
    
    fn();
  }
  
  template<typename Fn>
  void during_level(
      std::integral_constant<progress_level, progress_level::user>,
      Fn &&fn,
      persona &active_per
    ) {
    detail::persona_tls &tls = detail::the_persona_tls;
    
    // TODO: revisit the purpose of this seemingly wrong assertion
    //UPCXX_ASSERT(!UPCXX_BACKEND_GASNET_SEQ || backend::master.active_with_caller(tls));
    //persona &active_per = UPCXX_BACKEND_GASNET_SEQ
    //  ? backend::master
    //  : *tls.get_top_persona();
    
    tls.during(
      active_per, progress_level::user, std::forward<Fn>(fn),
      /*known_active=*/std::true_type{}
    );
  }

  template<progress_level level, typename Fn>
  void during_level(Fn &&fn, persona &active_per) {
    during_level(
      std::integral_constant<progress_level,level>{},
      std::forward<Fn>(fn),
      active_per
    );
  }
  
  //////////////////////////////////////////////////////////////////////
  // send_am_{master|persona}
  
  template<upcxx::progress_level level, typename Fn1>
  void send_am_master(team &tm, intrank_t recipient, Fn1 &&fn) {
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
        rpc_as_lpc::reader_of,
        rpc_as_lpc::template cleanup</*never_rdzv=*/definitely_eager>
      >(w, ub.size, fn);

    if(eager)
      gasnet::send_am_eager_master(level, tm, recipient, buf, w.size(), w.align());
    else
      gasnet::template send_am_rdzv<level>(tm, recipient, /*master*/nullptr, buf, w.size(), w.align());
  }
  
  template<upcxx::progress_level level, typename Fn>
  void send_am_persona(
      team &tm,
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
        rpc_as_lpc::reader_of,
        rpc_as_lpc::template cleanup</*never_rdzv=*/definitely_eager>
      >(w, ub.size, fn);
    
    if(eager)
      gasnet::send_am_eager_persona(level, tm, recipient_rank, recipient_persona, buf, w.size(), w.align());
    else
      gasnet::send_am_rdzv<level>(tm, recipient_rank, recipient_persona, buf, w.size(), w.align());
  }
  
  template<progress_level level, typename Fn1>
  void bcast_am_master(team &tm, Fn1 &&fn) {
    UPCXX_ASSERT(!UPCXX_BACKEND_GASNET_SEQ || backend::master.active_with_caller());
    
    using gasnet::eager_buffer;
    using gasnet::bcast_as_lpc;
    
    using Fn = typename std::decay<Fn1>::type;
    
    auto ub = command<detail::lpc_base*>::ubound(
      parcel_size_empty()
        .trivial_added<team_id>()
        .trivial_added<intrank_t>(),
      fn
    );
    
    constexpr bool definitely_eager = ub.static_size <= gasnet::am_size_rdzv_cutover_min;
    bool eager = ub.size <= gasnet::am_size_rdzv_cutover_min &&
                 ub.size <= gasnet::am_size_rdzv_cutover;
    
    eager_buffer<decltype(ub)> ebuf;
    void *buf;
    
    if(eager)
      buf = ebuf.allocate(ub);
    else {
      auto ub1 = ub.template trivial_added<std::atomic<std::int64_t>>();
      buf = upcxx::allocate(ub1.size, ub1.align);
    }
    
    parcel_writer w(buf);
    w.put_trivial_aligned<team_id>(tm.id());
    w.place_trivial_aligned<intrank_t>();
    
    command<detail::lpc_base*>::template pack<
        bcast_as_lpc::reader_of,
        bcast_as_lpc::template cleanup</*never_rdzv=*/definitely_eager>
      >(w, ub.size, fn);
    
    if(eager) {
      gasnet::bcast_am_master_eager(
          level, tm, tm.rank_me() + tm.rank_n(),
          buf, w.size(), w.align()
        );
    }
    else {
      std::atomic<std::int64_t> *rdzv_refs = new(
          w.place_trivial_aligned<std::atomic<std::int64_t>>()
        ) std::atomic<std::int64_t>(1<<30);
      
      int refs_added = gasnet::template bcast_am_master_rdzv<level>(
          tm,
          /*rank_d_ub*/tm.rank_me() + tm.rank_n(),
          /*rank_owner*/backend::rank_me,
          /*payload_sender*/buf,
          /*refs_sender*/rdzv_refs,
          w.size(), w.align()
        );
      
      int64_t refs_now = rdzv_refs->fetch_add(refs_added - (1<<30), std::memory_order_acq_rel);
      refs_now += refs_added - (1<<30);
      if(0 == refs_now)
        upcxx::deallocate(buf);
    }
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
  
  inline parcel_reader restricted_reader_of(void *p) { return parcel_reader(p); }
  inline void restricted_cleanup(void *p) {/*nop*/}
  
  template<typename Fn>
  void send_am_restricted(team &tm, intrank_t recipient, Fn &&fn) {
    UPCXX_ASSERT(!UPCXX_BACKEND_GASNET_SEQ || backend::master.active_with_caller());
    
    auto ub = command<detail::lpc_base*>::ubound(parcel_size_empty(), fn);
    eager_buffer<decltype(ub)> ebuf;
    void *buf = ebuf.allocate(ub);
    
    parcel_writer w(buf);
    command<void*>::pack<
        restricted_reader_of,
        restricted_cleanup
      >(w, ub.size, fn);
    
    gasnet::send_am_eager_restricted(tm, recipient, buf, w.size(), w.align());
  }
  
  //////////////////////////////////////////////////////////////////////
  // rpc_as_lpc
  
  template<>
  inline void rpc_as_lpc::cleanup</*never_rdzv=*/true>(detail::lpc_base *me1) {
    rpc_as_lpc *me = static_cast<rpc_as_lpc*>(me1);
    std::free(me->payload);
  }
  
  template<>
  inline void bcast_as_lpc::cleanup</*never_rdzv=*/true>(detail::lpc_base *me1) {
    bcast_as_lpc *me = static_cast<bcast_as_lpc*>(me1);
    if(0 == --me->eager_refs)
      std::free(me->payload);
  }
}}}

////////////////////////////////////////////////////////////////////////////////
// Bring in all backend definitions in case they haven't been included yet.
#include <upcxx/backend.hpp>

#endif
