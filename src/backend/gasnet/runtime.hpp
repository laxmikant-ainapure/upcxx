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

  struct bcast_payload_header;
  
  void bcast_am_master_eager(
    progress_level level,
    team &tm,
    intrank_t rank_d_ub, // in range [0, 2*rank_n-1)
    bcast_payload_header *payload,
    size_t cmd_size,
    size_t cmd_align
  );
  void bcast_am_master_rdzv(
    progress_level level,
    team &tm,
    intrank_t rank_d_ub, // in range [0, 2*rank_n-1)
    intrank_t wrank_owner, // world team coordinates
    bcast_payload_header *payload_owner, // owner address of payload
    bcast_payload_header *payload_sender, // sender (my) address of payload
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
    static detail::serialization_reader reader_of(detail::lpc_base *me) {
      return detail::serialization_reader(static_cast<rpc_as_lpc*>(me)->payload);
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

  struct bcast_payload_header {
    team_id tm_id;
    union {
      int eager_subrank_ub;
      std::atomic<std::int64_t> rdzv_refs;
    };
  };
  
  // The receiver-side bcast'd rpc message type. Inherits lpc base type since rpc's
  // reside in the lpc queues.
  struct bcast_as_lpc: rpc_as_lpc {
    int eager_refs;
    
    static detail::serialization_reader reader_of(detail::lpc_base *me) {
      detail::serialization_reader r(static_cast<bcast_as_lpc*>(me)->payload);
      r.unplace(storage_size_of<bcast_payload_header>());
      return r;
    }
    
    template<bool never_rdzv>
    static void cleanup(detail::lpc_base *me);
  };
  
  template<typename Ub,
           bool is_static_and_small = (Ub::static_size <= 1024)>
  struct rpc_out_buffer;

  template<>
  struct rpc_out_buffer</*Ub=*/invalid_storage_size_t, /*is_static_and_small=*/false> {
    bool is_eager;
    void *buffer;
    typename std::aligned_storage<512, serialization_align_max>::type tiny_;
    
    detail::serialization_writer</*bounded=*/false> prepare_writer(invalid_storage_size_t) {
      return detail::serialization_writer<false>(&tiny_, sizeof(tiny_));
    }
    
    void finalize_buffer(detail::serialization_writer<false> &&w) {
      is_eager = w.size() <= gasnet::am_size_rdzv_cutover_min ||
                 w.size() <= gasnet::am_size_rdzv_cutover;
      
      if(is_eager && w.contained_in_initial())
        buffer = &tiny_;
      else {
        if(is_eager)
          buffer = detail::alloc_aligned(w.size(), w.align());
        else
          buffer = upcxx::allocate(w.size(), w.align());
        
        w.compact_and_invalidate(buffer);
      }
    }

    ~rpc_out_buffer() {
      if(is_eager && buffer != (void*)&tiny_)
        std::free(buffer);
    }
  };

  template<typename Ub>
  struct rpc_out_buffer<Ub, /*is_static_and_small=*/false> {
    bool is_eager;
    void *buffer;
    typename std::aligned_storage<
        512,
        Ub::static_align_ub < serialization_align_max ? Ub::static_align_ub : serialization_align_max
      >::type tiny_;
    
    detail::serialization_writer</*bounded=*/true> prepare_writer(Ub ub) {
      is_eager = ub.size <= gasnet::am_size_rdzv_cutover_min ||
                 ub.size <= gasnet::am_size_rdzv_cutover;
      
      if(is_eager) {
        if(ub.size <= 512 && ub.align <= serialization_align_max)
          buffer = &tiny_;
        else
          buffer = detail::alloc_aligned(ub.size, ub.align);
      }
      else
        buffer = upcxx::allocate(ub.size, ub.align);
      
      return detail::serialization_writer<true>(buffer);
    }

    void finalize_buffer(detail::serialization_writer<true>&&) {}
    
    ~rpc_out_buffer() {
      if(is_eager && buffer != (void*)&tiny_)
        std::free(buffer);
    }
  };

  template<typename Ub>
  struct rpc_out_buffer<Ub, /*is_static_and_small=*/true> {
    typename std::aligned_storage<Ub::static_size, Ub::static_align>::type buf_;
    static constexpr bool is_eager = true;
    void *const buffer = &buf_;
    
    detail::serialization_writer</*bounded=*/true> prepare_writer(Ub) {
      return detail::serialization_writer<true>(&buf_);
    }
    void finalize_buffer(detail::serialization_writer<true>&&) {}
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
    
    using gasnet::rpc_out_buffer;
    using gasnet::rpc_as_lpc;
    
    using Fn = typename std::decay<Fn1>::type;
    
    auto ub = detail::command<detail::lpc_base*>::ubound(empty_storage_size, fn);
    
    constexpr bool definitely_eager = ub.static_size <= gasnet::am_size_rdzv_cutover_min;
    
    rpc_out_buffer<decltype(ub)> obuf;
    auto w = obuf.prepare_writer(ub);
    
    detail::command<detail::lpc_base*>::template serialize<
        rpc_as_lpc::reader_of,
        rpc_as_lpc::template cleanup</*never_rdzv=*/definitely_eager>
      >(w, ub.size, fn);

    std::size_t buf_size = w.size(), buf_align = w.align();
    obuf.finalize_buffer(std::move(w));
    
    if(obuf.is_eager)
      gasnet::send_am_eager_master(level, tm, recipient, obuf.buffer, buf_size, buf_align);
    else
      gasnet::template send_am_rdzv<level>(tm, recipient, /*master*/nullptr, obuf.buffer, buf_size, buf_align);
  }
  
  template<upcxx::progress_level level, typename Fn>
  void send_am_persona(
      team &tm,
      intrank_t recipient_rank,
      persona *recipient_persona,
      Fn &&fn
    ) {
    UPCXX_ASSERT(!UPCXX_BACKEND_GASNET_SEQ || backend::master.active_with_caller());
    
    using gasnet::rpc_out_buffer;
    using gasnet::rpc_as_lpc;
    
    auto ub = detail::command<detail::lpc_base*>::ubound(empty_storage_size, fn);
    
    constexpr bool definitely_eager = ub.static_size <= gasnet::am_size_rdzv_cutover_min;

    rpc_out_buffer<decltype(ub)> obuf;
    auto w = obuf.prepare_writer(ub);
    
    detail::command<detail::lpc_base*>::template serialize<
        rpc_as_lpc::reader_of,
        rpc_as_lpc::template cleanup</*never_rdzv=*/definitely_eager>
      >(w, ub.size, fn);

    std::size_t buf_size = w.size(), buf_align = w.align();
    obuf.finalize_buffer(std::move(w));
    
    if(obuf.is_eager)
      gasnet::send_am_eager_persona(level, tm, recipient_rank, recipient_persona, obuf.buffer, buf_size, buf_align);
    else
      gasnet::send_am_rdzv<level>(tm, recipient_rank, recipient_persona, obuf.buffer, buf_size, buf_align);
  }
  
  template<progress_level level, typename Fn1>
  void bcast_am_master(team &tm, Fn1 &&fn) {
    UPCXX_ASSERT(!UPCXX_BACKEND_GASNET_SEQ || backend::master.active_with_caller());
    
    using gasnet::rpc_out_buffer;
    using gasnet::bcast_as_lpc;
    using gasnet::bcast_payload_header;
    
    using Fn = typename std::decay<Fn1>::type;
    
    auto ub = detail::command<detail::lpc_base*>::ubound(
      empty_storage_size.cat_size_of<bcast_payload_header>(),
      fn
    );
    
    constexpr bool definitely_eager = ub.static_size <= gasnet::am_size_rdzv_cutover_min;

    rpc_out_buffer<decltype(ub)> obuf;

    auto w = obuf.prepare_writer(ub);
    w.place(storage_size_of<bcast_payload_header>());
    
    detail::command<detail::lpc_base*>::template serialize<
        bcast_as_lpc::reader_of,
        bcast_as_lpc::template cleanup</*never_rdzv=*/definitely_eager>
      >(w, ub.size, fn);

    std::size_t buf_size = w.size(), buf_align = w.align();
    obuf.finalize_buffer(std::move(w));

    bcast_payload_header *payload = new(obuf.buffer) bcast_payload_header;
    payload->tm_id = tm.id();
    
    if(obuf.is_eager) {
      gasnet::bcast_am_master_eager(
          level, tm, tm.rank_me() + tm.rank_n(),
          payload, buf_size, buf_align
        );
    }
    else {
      new(&payload->rdzv_refs) std::atomic<std::int64_t>(0);
      
      gasnet::bcast_am_master_rdzv(
          level, tm,
          /*rank_d_ub*/tm.rank_me() + tm.rank_n(),
          /*rank_owner*/backend::rank_me,
          /*payload_owner/sender*/payload, payload,
          buf_size, buf_align
        );
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
  
  inline detail::serialization_reader restricted_reader_of(void *p) {
    return detail::serialization_reader(p);
  }
  inline void restricted_cleanup(void *p) {/*nop*/}
  
  template<typename Fn>
  void send_am_restricted(team &tm, intrank_t recipient, Fn &&fn) {
    UPCXX_ASSERT(!UPCXX_BACKEND_GASNET_SEQ || backend::master.active_with_caller());
    
    auto ub = detail::command<detail::lpc_base*>::ubound(empty_storage_size, fn);
    rpc_out_buffer<decltype(ub)> obuf;

    auto w = obuf.prepare_writer(ub);
    detail::command<void*>::serialize<
        restricted_reader_of,
        restricted_cleanup
      >(w, ub.size, fn);
    
    std::size_t buf_size = w.size(), buf_align = w.align();
    obuf.finalize_buffer(std::move(w));
    
    gasnet::send_am_eager_restricted(tm, recipient, obuf.buffer, buf_size, buf_align);
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
