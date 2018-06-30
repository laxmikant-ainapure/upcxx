#include <upcxx/backend/gasnet/runtime.hpp>
#include <upcxx/backend/gasnet/runtime_internal.hpp>

#include <upcxx/os_env.hpp>
#include <upcxx/team.hpp>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>

#include <sched.h>
#include <unistd.h>

namespace backend = upcxx::backend;
namespace detail  = upcxx::detail;
namespace gasnet  = upcxx::backend::gasnet;

using upcxx::command;
using upcxx::intrank_t;
using upcxx::parcel_reader;
using upcxx::parcel_writer;
using upcxx::persona;
using upcxx::persona_scope;
using upcxx::progress_level;

using backend::persona_state;

using gasnet::handle_cb_queue;
using gasnet::rpc_as_lpc;

using namespace std;

////////////////////////////////////////////////////////////////////////

#if !NOBS_DISCOVERY
  #if UPCXX_BACKEND_GASNET_SEQ && !GASNET_SEQ
    #error "This backend is gasnet-seq only!"
  #endif

  #if UPCXX_BACKEND_GASNET_PAR && !GASNET_PAR
    #error "This backend is gasnet-par only!"
  #endif

  #if GASNET_SEGMENT_EVERYTHING
    #error "Segment-everything not supported."
  #endif
#endif

static_assert(
  sizeof(gex_Event_t) == sizeof(uintptr_t),
  "Failed: sizeof(gex_Event_t) == sizeof(uintptr_t)"
);

////////////////////////////////////////////////////////////////////////
// from: upcxx/backend.hpp

int backend::init_count = 0;
  
intrank_t backend::rank_n = -1;
intrank_t backend::rank_me; // leave undefined so valgrind can catch it.

persona backend::master;
persona_scope *backend::initial_master_scope = nullptr;

intrank_t backend::pshm_peer_lb;
intrank_t backend::pshm_peer_ub;
intrank_t backend::pshm_peer_n;

unique_ptr<uintptr_t[/*local_team.size()*/]> backend::pshm_local_minus_remote;
unique_ptr<uintptr_t[/*local_team.size()*/]> backend::pshm_vbase;
unique_ptr<uintptr_t[/*local_team.size()*/]> backend::pshm_size;

////////////////////////////////////////////////////////////////////////
// from: upcxx/backend/gasnet/runtime.hpp

size_t gasnet::am_size_rdzv_cutover;

#if UPCXX_BACKEND_GASNET_SEQ
  handle_cb_queue gasnet::master_hcbs;
#endif

////////////////////////////////////////////////////////////////////////
// from: upcxx/backend/gasnet/runtime_internal.hpp

gex_TM_t gasnet::world_team;

////////////////////////////////////////////////////////////////////////

namespace {
  // List of {vbase, peer} pairs (in seperate arrays) sorted by `vbase`, where
  // `vbase` is the local virt-address base for peer segments and `peer` is the
  // local peer index owning that segment.
  unique_ptr<uintptr_t[/*local_team.size()*/]> pshm_owner_vbase;
  unique_ptr<intrank_t[/*local_team.size()*/]> pshm_owner_peer;

  #if UPCXX_BACKEND_GASNET_SEQ
    // Set by the thread which initiates gasnet since in SEQ only that thread
    // may invoke gasnet.
    void *gasnet_seq_thread_id = nullptr;
  #else
    // unused
    constexpr void *gasnet_seq_thread_id = nullptr;
  #endif
}

////////////////////////////////////////////////////////////////////////

namespace {
  enum {
    id_am_eager_restricted = GEX_AM_INDEX_BASE,
    id_am_eager_master,
    id_am_eager_persona
  };
    
  void am_eager_restricted(gex_Token_t, void *buf, size_t buf_size, gex_AM_Arg_t buf_align);
  void am_eager_master(gex_Token_t, void *buf, size_t buf_size, gex_AM_Arg_t buf_align_and_level);
  void am_eager_persona(gex_Token_t, void *buf, size_t buf_size, gex_AM_Arg_t buf_align_and_level,
                        gex_AM_Arg_t persona_ptr_lo, gex_AM_Arg_t persona_ptr_hi);

  #define AM_ENTRY(name, arg_n) \
    {id_##name, (void(*)())name, GEX_FLAG_AM_MEDIUM | GEX_FLAG_AM_REQUEST, arg_n, nullptr, #name}
  
  gex_AM_Entry_t am_table[] = {
    AM_ENTRY(am_eager_restricted, 1),
    AM_ENTRY(am_eager_master, 1),
    AM_ENTRY(am_eager_persona, 3)
  };
}

////////////////////////////////////////////////////////////////////////

#include <upcxx/dl_malloc.h>

namespace {
  std::mutex segment_lock_;
  mspace segment_mspace_;
}
  
////////////////////////////////////////////////////////////////////////
// from: upcxx/backend.hpp

void upcxx::init() {
  if(0 != backend::init_count++)
    return;
  
  int ok;

  #if UPCXX_BACKEND_GASNET_SEQ
    gasnet_seq_thread_id = upcxx::detail::thread_id();
  #endif

  gex_Client_t client;
  gex_EP_t endpoint;
  gex_Segment_t segment;

  ok = gex_Client_Init(
    &client, &endpoint, &gasnet::world_team,
    "upcxx", nullptr, nullptr, 0
  );
  UPCXX_ASSERT_ALWAYS(ok == GASNET_OK);

  size_t segment_size = size_t(os_env<double>("UPCXX_SEGMENT_MB", 128)*(1<<20));
  // page size should always be a power of 2
  segment_size = (segment_size + GASNET_PAGESIZE-1) & -GASNET_PAGESIZE;
  
  backend::rank_n = gex_TM_QuerySize(gasnet::world_team);
  backend::rank_me = gex_TM_QueryRank(gasnet::world_team);

  // Build team upcxx::world()
  ::new(&detail::the_world_team.raw) upcxx::team(
    detail::internal_only(),
    0, backend::rank_n
  );
  
  // now adjust the segment size if it's less than the GASNET_MAX_SEGSIZE
  size_t gasnet_max_segsize = gasnet_getMaxLocalSegmentSize();
  if(segment_size > gasnet_max_segsize) {
    if(upcxx::rank_me() == 0) {
      cerr << "WARNING: Requested UPC++ segment size (" << segment_size << ") "
              "is larger than the GASNet segment size (" << gasnet_max_segsize << "). "
              "Adjusted segment size to " << (gasnet_max_segsize) << ".\n";
    }
    segment_size = gasnet_max_segsize;
  }
  
  backend::initial_master_scope = new persona_scope(backend::master);
  UPCXX_ASSERT_ALWAYS(backend::master.active_with_caller());
  
  ok = gex_Segment_Attach(&segment, gasnet::world_team, segment_size);
  UPCXX_ASSERT_ALWAYS(ok == GASNET_OK);

  ok = gex_EP_RegisterHandlers(endpoint, am_table, sizeof(am_table)/sizeof(am_table[0]));
  UPCXX_ASSERT_ALWAYS(ok == GASNET_OK);

  size_t am_medium_size = gex_AM_MaxRequestMedium(
    gasnet::world_team,
    GEX_RANK_INVALID,
    GEX_EVENT_NOW,
    /*flags*/0,
    3
  );
  
  /* TODO: I pulled this from thin air. We want to lean towards only
   * sending very small messages eagerly so as not to clog the landing
   * zone, which would force producers to block on their next send. By
   * using a low threshold for rendezvous we increase the probability
   * of there being enough landing space to send the notification of
   * yet another rendezvous. I'm using the max medium size as a heuristic
   * means to approximate the landing zone size. This is not at all
   * accurate, we should be doing something conduit dependent.
   */
  gasnet::am_size_rdzv_cutover =
    am_medium_size < 1<<10 ? 256 :
    am_medium_size < 8<<10 ? 512 :
                             1024;
  UPCXX_ASSERT(gasnet::am_size_rdzv_cutover_min <= gasnet::am_size_rdzv_cutover);
  
  // setup shared segment allocator
  void *segment_base;
  
  ok = gex_Segment_QueryBound(
    gasnet::world_team, backend::rank_me,
    &segment_base, nullptr, &segment_size
  );
  UPCXX_ASSERT_ALWAYS(ok == GASNET_OK);
  
  segment_mspace_ = create_mspace_with_base(segment_base, segment_size, 1);
  mspace_set_footprint_limit(segment_mspace_, segment_size);

  //////////////////////////////////////////////////////////////////////////////
  // Setup the local-memory neighborhood tables.
  
  gex_RankInfo_t *nbhd;
  gex_Rank_t peer_n, peer_me;
  gex_System_QueryNbrhdInfo(&nbhd, &peer_n, &peer_me);

  bool contiguous_nbhd = true;
  for(gex_Rank_t p=1; p < peer_n; p++)
    contiguous_nbhd &= (nbhd[p].gex_jobrank == 1 + nbhd[p-1].gex_jobrank);

  if(!contiguous_nbhd) {
    // Discontiguous rank-set is collapsed to singleton set of "me"
    backend::pshm_peer_lb = backend::rank_me;
    backend::pshm_peer_ub = backend::rank_me + 1;
    peer_n = 1;
    peer_me = 0;
  }
  else {
    backend::pshm_peer_lb = nbhd[0].gex_jobrank;
    backend::pshm_peer_ub = nbhd[0].gex_jobrank + peer_n;
  }
  backend::pshm_peer_n = peer_n;
  
  // Build upcxx::local_team()
  ::new(&detail::the_local_team.raw) upcxx::team(
    detail::internal_only(),
    backend::pshm_peer_lb,
    backend::pshm_peer_ub
  );
  
  backend::pshm_local_minus_remote.reset(new uintptr_t[peer_n]);
  backend::pshm_vbase.reset(new uintptr_t[peer_n]);
  backend::pshm_size.reset(new uintptr_t[peer_n]);
  pshm_owner_vbase.reset(new uintptr_t[peer_n]);
  pshm_owner_peer.reset(new intrank_t[peer_n]);
  
  for(gex_Rank_t p=0; p < peer_n; p++) {
    void *owner_vbase, *local_vbase;
    uintptr_t size;

    gex_Segment_QueryBound(
      /*team*/gasnet::world_team,
      /*rank*/backend::pshm_peer_lb + p,
      &owner_vbase, &local_vbase, &size
    );
    
    backend::pshm_local_minus_remote[p] = reinterpret_cast<uintptr_t>(local_vbase) - reinterpret_cast<uintptr_t>(owner_vbase);
    backend::pshm_vbase[p] = reinterpret_cast<uintptr_t>(local_vbase);
    backend::pshm_size[p] = size;
    
    pshm_owner_peer[p] = p; // initialize peer indices as identity permutation
  }

  // Sort peer indices according to their vbase. We use `std::qsort` instead of
  // `std::sort` because performance is not critical and qsort *hopefully*
  // generates a lot less code in the executable binary.
  std::qsort(
    /*first*/pshm_owner_peer.get(),
    /*count*/peer_n,
    /*size*/sizeof(intrank_t),
    /*compare*/[](void const *pa, void const *pb)->int {
      intrank_t a = *static_cast<intrank_t const*>(pa);
      intrank_t b = *static_cast<intrank_t const*>(pb);

      uintptr_t va = backend::pshm_vbase[a];
      uintptr_t vb = backend::pshm_vbase[b];
      
      return va < vb ? -1 : va == vb ? 0 : +1;
    }
  );

  // permute vbase's into sorted order
  for(gex_Rank_t i=0; i < peer_n; i++)
    pshm_owner_vbase[i] = backend::pshm_vbase[pshm_owner_peer[i]];

  //////////////////////////////////////////////////////////////////////////////
  // Exit barrier
  
  gasnet_barrier_notify(0, GASNET_BARRIERFLAG_ANONYMOUS);
  ok = gasnet_barrier_wait(0, GASNET_BARRIERFLAG_ANONYMOUS);
  UPCXX_ASSERT_ALWAYS(ok == GASNET_OK);
}

void upcxx::finalize() {
  UPCXX_ASSERT_ALWAYS(backend::init_count > 0);
  
  if(0 != --backend::init_count)
    return;
  
  upcxx::barrier();

  detail::the_world_team.destruct();
  detail::the_local_team.destruct();
  
  if(backend::initial_master_scope != nullptr)
    delete backend::initial_master_scope;
}

void upcxx::liberate_master_persona() {
  UPCXX_ASSERT_ALWAYS(&upcxx::current_persona() == &backend::master);
  UPCXX_ASSERT_ALWAYS(backend::initial_master_scope != nullptr);
  
  delete backend::initial_master_scope;
  
  backend::initial_master_scope = nullptr;
}

void upcxx::barrier() {
  UPCXX_ASSERT(backend::master.active_with_caller());
  
  gasnet_barrier_notify(0, GASNET_BARRIERFLAG_ANONYMOUS);
  
  while(GASNET_OK != gasnet_barrier_try(0, GASNET_BARRIERFLAG_ANONYMOUS))
    upcxx::progress();
}

void* upcxx::allocate(size_t size, size_t alignment) {
  #if UPCXX_BACKEND_GASNET_SEQ
    UPCXX_ASSERT(backend::master.active_with_caller());
  #elif UPCXX_BACKEND_GASNET_PAR
    std::lock_guard<std::mutex> locked{segment_lock_};
  #endif
  
  void *p = mspace_memalign(segment_mspace_, alignment, size);
  //UPCXX_ASSERT(p != nullptr);
  return p;
}

void upcxx::deallocate(void *p) {
  #if UPCXX_BACKEND_GASNET_SEQ
    UPCXX_ASSERT(backend::master.active_with_caller());
  #elif UPCXX_BACKEND_GASNET_PAR
    std::lock_guard<std::mutex> locked{segment_lock_};
  #endif
  
  mspace_free(segment_mspace_, p);
}

//////////////////////////////////////////////////////////////////////
// from: upcxx/backend.hpp

tuple<intrank_t/*rank*/, uintptr_t/*raw*/> backend::globalize_memory(void const *addr) {
  intrank_t peer_n = pshm_peer_ub - pshm_peer_lb;
  uintptr_t uaddr = reinterpret_cast<uintptr_t>(addr);

  // key is a pointer to one past the last vbase less-or-equal to addr.
  uintptr_t *key = std::upper_bound(
    pshm_owner_vbase.get(),
    pshm_owner_vbase.get() + peer_n,
    uaddr
  );

  int key_ix = key - pshm_owner_vbase.get();

  #define bad_memory "Local memory "<<addr<<" is not in any local rank's shared segment."

  UPCXX_ASSERT(key_ix > 0, bad_memory);
  
  intrank_t peer = pshm_owner_peer[key_ix-1];

  UPCXX_ASSERT(uaddr - pshm_vbase[peer] <= pshm_size[peer], bad_memory);
  
  return std::make_tuple(
    pshm_peer_lb + peer,
    uaddr - pshm_local_minus_remote[peer]
  );

  #undef bad_memory
}

////////////////////////////////////////////////////////////////////////
// from: upcxx/backend/gasnet/runtime.hpp

void gasnet::send_am_eager_restricted(
    intrank_t recipient,
    void *buf,
    std::size_t buf_size,
    std::size_t buf_align
  ) {
  
  gex_AM_RequestMedium1(
    world_team, recipient,
    id_am_eager_restricted, buf, buf_size,
    GEX_EVENT_NOW, /*flags*/0,
    buf_align
  );
  
  after_gasnet();
}

void gasnet::send_am_eager_master(
    progress_level level,
    intrank_t recipient,
    void *buf,
    std::size_t buf_size,
    std::size_t buf_align
  ) {
  
  gex_AM_RequestMedium1(
    world_team, recipient,
    id_am_eager_master, buf, buf_size,
    GEX_EVENT_NOW, /*flags*/0,
    buf_align<<1 | (level == progress_level::user ? 1 : 0)
  );
  
  after_gasnet();
}

void gasnet::send_am_eager_persona(
    progress_level level,
    intrank_t recipient_rank,
    persona *recipient_persona,
    void *buf,
    std::size_t buf_size,
    std::size_t buf_align
  ) {
  
  gex_AM_Arg_t per_lo = reinterpret_cast<intptr_t>(recipient_persona) & 0xffffffffu;
  gex_AM_Arg_t per_hi = reinterpret_cast<intptr_t>(recipient_persona) >> 31 >> 1;

  gex_AM_RequestMedium3(
    world_team, recipient_rank,
    id_am_eager_persona, buf, buf_size,
    GEX_EVENT_NOW, /*flags*/0,
    buf_align<<1 | (level == progress_level::user ? 1 : 0),
    per_lo, per_hi
  );
  
  after_gasnet();
}

namespace {
  template<typename Fn>
  struct rma_get_cb final: gasnet::handle_cb {
    Fn fn_;
    rma_get_cb(Fn fn): fn_(std::move(fn)) {}

    void execute_and_delete(gasnet::handle_cb_successor add) {
      fn_();
      delete this;
    }
  };

  template<typename Fn>
  void rma_get(
      void *buf_d,
      intrank_t rank_s,
      void const *buf_s,
      size_t buf_size,
      Fn fn
    ) {
    
    UPCXX_ASSERT(!UPCXX_BACKEND_GASNET_SEQ || backend::master.active_with_caller());

    auto *cb = new rma_get_cb<Fn>{std::move(fn)};
    
    gex_Event_t h = gex_RMA_GetNB(
      gasnet::world_team,
      buf_d, rank_s, const_cast<void*>(buf_s), buf_size,
      /*flags*/0
    );
    cb->handle = reinterpret_cast<uintptr_t>(h);
    
    gasnet::register_cb(cb);
    gasnet::after_gasnet();
  }
}

template<progress_level level>
void gasnet::send_am_rdzv(
    intrank_t rank_d,
    persona *persona_d,
    void *buf_s,
    size_t cmd_size,
    size_t cmd_align
  ) {
  
  intrank_t rank_s = backend::rank_me;
  
  backend::send_am_persona<progress_level::internal>(
    rank_d,
    persona_d,
    [=]() {
      if(backend::rank_is_local(rank_s)) {
        void *payload = backend::localize_memory_nonnull(rank_s, reinterpret_cast<std::uintptr_t>(buf_s));
        
        rpc_as_lpc *m = new rpc_as_lpc;
        m->the_vtbl.execute_and_delete = command<detail::lpc_base*>::get_executor(payload);
        m->vtbl = &m->the_vtbl;
        m->payload = payload;
        m->is_rdzv = true;
        m->rank_s = rank_s;
        
        auto &tls = detail::the_persona_tls;
        tls.enqueue(*tls.get_top_persona(), level, m, /*known_active=*/std::true_type());
      }
      else {
        rpc_as_lpc *m = rpc_as_lpc::build_rdzv_lz(rank_s, cmd_size, cmd_align);
        
        rma_get(
          m->payload, rank_s, buf_s, cmd_size,
          [=]() {
            auto &tls = detail::the_persona_tls;
            
            m->the_vtbl.execute_and_delete = command<detail::lpc_base*>::get_executor(m->payload);
            tls.enqueue(*tls.get_top_persona(), level, m, /*known_active=*/std::true_type());
            
            // Notify source rank it can free buffer.
            gasnet::send_am_restricted(rank_s,
              [=]() { upcxx::deallocate(buf_s); }
            );
          }
        );
      }
    }
  );
}

template void gasnet::send_am_rdzv<progress_level::internal>(intrank_t, persona*, void*, size_t, size_t);
template void gasnet::send_am_rdzv<progress_level::user>(intrank_t, persona*, void*, size_t, size_t);

template<>
void rpc_as_lpc::cleanup</*never_rdzv=*/false>(detail::lpc_base *me1) {
  rpc_as_lpc *me = static_cast<rpc_as_lpc*>(me1);
  
  if(!me->is_rdzv)
    std::free(me->payload);
  else {
    if(backend::rank_is_local(me->rank_s)) {
      // Notify source rank it can free buffer.
      std::uintptr_t buf_s = backend::globalize_memory_nonnull(me->rank_s, me->payload);
      
      send_am_restricted(me->rank_s,
        [=]() { upcxx::deallocate(reinterpret_cast<void*>(buf_s)); }
      );
      
      delete me;
    }
    else
      upcxx::deallocate(me->payload);
  }
}

inline rpc_as_lpc* rpc_as_lpc::build_copy(
    void *cmd_buf,
    std::size_t cmd_size,
    std::size_t cmd_alignment
  ) {
  
  std::size_t msg_size = cmd_size;
  msg_size = (msg_size + alignof(rpc_as_lpc)-1) & -alignof(rpc_as_lpc);
  
  std::size_t msg_offset = msg_size;
  msg_size += sizeof(rpc_as_lpc);
  
  void *msg_buf;
  int ok = posix_memalign(&msg_buf, cmd_alignment, msg_size);
  UPCXX_ASSERT_ALWAYS(ok == 0);
  
  rpc_as_lpc *m = ::new((char*)msg_buf + msg_offset) rpc_as_lpc;
  m->the_vtbl.execute_and_delete = command<detail::lpc_base*>::get_executor(cmd_buf);
  m->vtbl = &m->the_vtbl;
  m->payload = msg_buf;
  m->is_rdzv = false;
  
  // The (void**) casts *might* inform memcpy that it can assume word
  // alignment.
  std::memcpy((void**)msg_buf, (void**)cmd_buf, cmd_size);
  
  return m;
}

inline rpc_as_lpc* rpc_as_lpc::build_rdzv_lz(
    intrank_t rank_s,
    std::size_t cmd_size,
    std::size_t cmd_alignment // alignment requirement of packing
  ) {
  std::size_t offset = (cmd_size + alignof(rpc_as_lpc)-1) & -alignof(rpc_as_lpc);
  std::size_t buf_size = offset + sizeof(rpc_as_lpc);
  std::size_t buf_align = std::max(cmd_alignment, alignof(rpc_as_lpc));
  
  void *buf = upcxx::allocate(buf_size, buf_align);
  UPCXX_ASSERT_ALWAYS(buf != nullptr);
  
  rpc_as_lpc *m = ::new((char*)buf + offset) rpc_as_lpc;
  m->the_vtbl.execute_and_delete = nullptr; // filled in when GET completes
  m->vtbl = &m->the_vtbl;
  m->payload = buf;
  m->is_rdzv = true;
  m->rank_s = rank_s;
  
  return m;
}

void gasnet::after_gasnet() {
  detail::persona_tls &tls = detail::the_persona_tls;
  
  if(tls.get_progressing() >= 0 || !tls.is_burstable(progress_level::internal))
    return;
  tls.set_progressing((int)progress_level::internal);
  
  int total_exec_n = 0;
  int exec_n;
  
  do {
    exec_n = 0;
    
    tls.foreach_active_as_top([&](persona &p) {
      #if UPCXX_BACKEND_GASNET_SEQ
        if(&p == &backend::master)
          exec_n += gasnet::master_hcbs.burst(4);
      #elif UPCXX_BACKEND_GASNET_PAR
        exec_n += p.backend_state_.hcbs.burst(4);
      #endif
      
      exec_n += tls.burst_internal(p);
    });
    
    total_exec_n += exec_n;
  }
  while(total_exec_n < 100 && exec_n != 0);
  //while(0);
  
  tls.set_progressing(-1);
}

////////////////////////////////////////////////////////////////////////
// from: upcxx/backend.hpp

int upcxx::detail::progressing() {
  return the_persona_tls.get_progressing();
}

void upcxx::progress(progress_level level) {
  detail::persona_tls &tls = detail::the_persona_tls;
  
  if(tls.get_progressing() >= 0)
    return;
  tls.set_progressing((int)level);
  
  if(level == progress_level::user)
    tls.flip_burstable(progress_level::user);
  
  int total_exec_n = 0;
  int exec_n;
  
  if(!UPCXX_BACKEND_GASNET_SEQ || gasnet_seq_thread_id == detail::thread_id())
    gasnet_AMPoll();
  
  do {
    exec_n = 0;
    
    tls.foreach_active_as_top([&](persona &p) {
      #if UPCXX_BACKEND_GASNET_SEQ
        if(&p == &backend::master)
          exec_n += gasnet::master_hcbs.burst(4);
      #elif UPCXX_BACKEND_GASNET_PAR
        exec_n += p.backend_state_.hcbs.burst(4);
      #endif
      
      exec_n += tls.burst_internal(p);
      
      if(level == progress_level::user) {
        tls.flip_burstable(progress_level::user);
        exec_n += tls.burst_user(p);
        tls.flip_burstable(progress_level::user);
      }
    });
    
    total_exec_n += exec_n;
  }
  // Try really hard to do stuff before leaving attentiveness.
  while(total_exec_n < 1000 && exec_n != 0);
  //while(0);
  
  #if 1
    /* In SMP tests we typically oversubscribe ranks to cpus. This is
     * an attempt at heuristically determining if this rank is just
     * spinning fruitlessly hogging the cpu from another who needs it.
     * It would be a lot more effective if we included knowledge of
     * whether outgoing communication was generated between progress
     * calls, then we would really know that we're just idle. Well,
     * almost. There would still exist the case where this rank is
     * receiving nothing, sending nothing, but is loaded with compute
     * and is only periodically progressing to be "nice".
     */
    static __thread int consecutive_nothings = 0;
    
    if(total_exec_n != 0)
      consecutive_nothings = 0;
    else if(++consecutive_nothings == 10) {
      sched_yield();
      consecutive_nothings = 0;
    }
  #endif
  
  tls.flip_burstable(progress_level::user);
  tls.set_progressing(-1);
}

////////////////////////////////////////////////////////////////////////
// anonymous namespace

namespace {
  void am_eager_restricted(
      gex_Token_t,
      void *buf, size_t buf_size,
      gex_AM_Arg_t buf_align
    ) {
    
    if(0 == (reinterpret_cast<uintptr_t>(buf) & (buf_align-1))) {
      command<void*>::get_executor(buf)(buf);
    }
    else {
      void *tmp;
      int ok = posix_memalign(&tmp, buf_align, buf_size);
      UPCXX_ASSERT_ALWAYS(ok == 0);
      
      std::memcpy((void**)tmp, (void**)buf, buf_size);
      
      command<void*>::get_executor(buf)(buf);
      
      std::free(tmp);
    }
  }
  
  void am_eager_master(
      gex_Token_t,
      void *buf, size_t buf_size,
      gex_AM_Arg_t buf_align_and_level
    ) {
    
    UPCXX_ASSERT(backend::rank_n != -1);
    
    size_t buf_align = buf_align_and_level>>1;
    bool level_user = buf_align_and_level & 1;
    
    rpc_as_lpc *m = rpc_as_lpc::build_copy(buf, buf_size, buf_align);
    
    detail::persona_tls &tls = detail::the_persona_tls;
    
    tls.enqueue(
      backend::master,
      level_user ? progress_level::user : progress_level::internal,
      m,
      /*known_active=*/std::integral_constant<bool, !UPCXX_BACKEND_GASNET_PAR>()
    );
  }
  
  void am_eager_persona(
      gex_Token_t,
      void *buf, size_t buf_size,
      gex_AM_Arg_t buf_align_and_level,
      gex_AM_Arg_t per_lo,
      gex_AM_Arg_t per_hi
    ) {
    
    UPCXX_ASSERT(backend::rank_n != -1);
    
    size_t buf_align = buf_align_and_level>>1;
    bool level_user = buf_align_and_level & 1;
    
    // Reconstructing a pointer from two gex_AM_Arg_t is nuanced
    // since the size of gex_AM_Arg_t is unspecified. The high
    // bits (per_hi) can be safely upshifted into place, on a 32-bit
    // system the result will just be zero. The low bits (per_lo) must
    // not be permitted to sign-extend. Masking against 0xf's achieves
    // this because all literals are non-negative. So the result of the
    // AND could either be signed or unsigned depending on if the mask
    // (a positive value) can be expressed in the desitination signed
    // type (intptr_t).
    persona *per = reinterpret_cast<persona*>(
      static_cast<intptr_t>(per_hi)<<31<<1 |
      (per_lo & 0xffffffff)
    );
    per = per == nullptr ? &backend::master : per; 
    
    rpc_as_lpc *m = rpc_as_lpc::build_copy(buf, buf_size, buf_align);
    
    detail::persona_tls &tls = detail::the_persona_tls;
    
    tls.enqueue(
      *per,
      level_user ? progress_level::user : progress_level::internal,
      m,
      /*known_active=*/std::integral_constant<bool, !UPCXX_BACKEND_GASNET_PAR>()
    );
  }
}

////////////////////////////////////////////////////////////////////////

inline int handle_cb_queue::burst(int burst_n) {
  int exec_n = 0;
  handle_cb **pp = &this->head_;
  
  while(burst_n-- && *pp != nullptr) {
    handle_cb *p = *pp;
    gex_Event_t ev = reinterpret_cast<gex_Event_t>(p->handle);
    
    if(0 == gex_Event_Test(ev)) {
      // remove from queue
      *pp = p->next_;
      if(*pp == nullptr)
        this->set_tailp(pp);
      
      // do it!
      p->execute_and_delete(handle_cb_successor{this, pp});
      
      exec_n += 1;
    }
    else
      pp = &p->next_;
  }
  
  return exec_n;
}

////////////////////////////////////////////////////////////////////////
// Library version watermarking

#include <upcxx/upcxx.hpp> // for UPCXX_VERSION

#ifndef UPCXX_VERSION
#error  UPCXX_VERSION missing!
#endif
GASNETT_IDENT(UPCXX_IdentString_LibraryVersion, "$UPCXXLibraryVersion: " _STRINGIFY(UPCXX_VERSION) " $");

#ifndef UPCXX_GIT_VERSION
#include <upcxx/git_version.h>
#endif
#ifdef  UPCXX_GIT_VERSION
GASNETT_IDENT(UPCXX_IdentString_GitVersion, "$UPCXXGitVersion: " _STRINGIFY(UPCXX_GIT_VERSION) " $");
#endif

