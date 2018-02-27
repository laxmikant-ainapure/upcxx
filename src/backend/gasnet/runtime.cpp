#include <upcxx/backend/gasnet/runtime.hpp>
#include <upcxx/backend/gasnet/runtime_internal.hpp>
#include <upcxx/backend/gasnet/rpc_inbox.hpp>

#include <upcxx/os_env.hpp>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <memory>

#include <sched.h>
#include <unistd.h>

namespace backend = upcxx::backend;
namespace detail  = upcxx::detail;
namespace gasnet  = upcxx::backend::gasnet;

using upcxx::future;
using upcxx::intrank_t;
using upcxx::parcel_reader;
using upcxx::parcel_writer;
using upcxx::persona;
using upcxx::persona_scope;
using upcxx::progress_level;

using backend::persona_state;

using gasnet::handle_cb_queue;
using gasnet::rpc_inbox;
using gasnet::rpc_message;

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

intrank_t backend::local_peer_lb;
intrank_t backend::local_peer_ub;

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
  // master-owned
  rpc_inbox rpcs_internal_;
  rpc_inbox rpcs_user_;

  // Given index in local_team:
  //   local_minus_remote: Encodes virtual address translation which is added
  //     to the raw encoding to get local virtual address.
  //   vbase: Local virtual address mapping to beginning of peer's segment
  //   size: Size of peer's segment in bytes.
  unique_ptr<uintptr_t[/*local_team.size()*/]> local_mem_local_minus_remote;
  unique_ptr<uintptr_t[/*local_team.size()*/]> local_mem_vbase;
  unique_ptr<uintptr_t[/*local_team.size()*/]> local_mem_size;

  // List of {vbase, peer} pairs (in seperate arrays) sorted by `vbase`, where
  // `vbase` is the local virt-address base for peer segments and `peer` is the
  // local peer index owning that segment.
  unique_ptr<uintptr_t[/*local_team.size()*/]> local_mem_owner_vbase;
  unique_ptr<intrank_t[/*local_team.size()*/]> local_mem_owner_peer;
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
  
  // now adjust the segment size if it's less than the GASNET_MAX_SEGSIZE
  size_t gasnet_max_segsize = gasnet_getMaxLocalSegmentSize();
  if(segment_size > gasnet_max_segsize) {
    if(upcxx::rank_me() == 0) {
      cerr << "WARNING: Requested UPCXX segment size (" << segment_size << ") "
              "is larger than the GASNet segment size (" << gasnet_max_segsize << "). "
              "Adjusted segment size to " << (gasnet_max_segsize) << ".\n";
    }
    segment_size = gasnet_max_segsize;
  }
  
  backend::initial_master_scope = new persona_scope{backend::master};
  
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
  gasnet_barrier_notify(0, GASNET_BARRIERFLAG_ANONYMOUS);
  ok = gasnet_barrier_wait(0, GASNET_BARRIERFLAG_ANONYMOUS);
  UPCXX_ASSERT_ALWAYS(ok == GASNET_OK);
  
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
  
  gex_NbrhdInfo_t *nbhd;
  gex_Rank_t peer_n, peer_me;
  gex_System_QueryNbrhdInfo(&nbhd, &peer_n, &peer_me);

  backend::local_peer_lb = nbhd[0].gex_jobrank;
  backend::local_peer_ub = nbhd[0].gex_jobrank + peer_n;
  
  local_mem_local_minus_remote.reset(new uintptr_t[peer_n]);
  local_mem_vbase.reset(new uintptr_t[peer_n]);
  local_mem_size.reset(new uintptr_t[peer_n]);
  local_mem_owner_vbase.reset(new uintptr_t[peer_n]);
  local_mem_owner_peer.reset(new intrank_t[peer_n]);
  
  for(gex_Rank_t p=0; p < peer_n; p++) {
    if(p != 0) {
      UPCXX_ASSERT_ALWAYS(
        nbhd[p].gex_jobrank == 1 + nbhd[p-1].gex_jobrank,
        "UPC++ internal error. GASNet reported a discontiguous set of job-ranks as local peers."
      );
    }
    
    void *owner_vbase, *local_vbase;
    uintptr_t size;

    gex_Segment_QueryBound(
      /*team*/gasnet::world_team,
      /*rank*/nbhd[0].gex_jobrank + p,
      &owner_vbase, &local_vbase, &size
    );
    
    local_mem_local_minus_remote[p] = reinterpret_cast<uintptr_t>(local_vbase) - reinterpret_cast<uintptr_t>(owner_vbase);
    local_mem_vbase[p] = reinterpret_cast<uintptr_t>(local_vbase);
    local_mem_size[p] = size;
    
    local_mem_owner_peer[p] = p; // initialize peer indices as identity permutation
  }

  // Sort peer indices according to their vbase. We use `std::qsort` instead of
  // `std::sort` because performance is not critical and qsort *hopefully*
  // generates a lot less code in the executable binary.
  std::qsort(
    /*first*/local_mem_owner_peer.get(),
    /*count*/peer_n,
    /*size*/sizeof(intrank_t),
    /*compare*/[](void const *pa, void const *pb)->int {
      intrank_t a = *static_cast<intrank_t const*>(pa);
      intrank_t b = *static_cast<intrank_t const*>(pb);

      uintptr_t va = local_mem_vbase[a];
      uintptr_t vb = local_mem_vbase[b];
      
      return va < vb ? -1 : va == vb ? 0 : +1;
    }
  );

  // permute vbase's into sorted order
  for(gex_Rank_t i=0; i < peer_n; i++)
    local_mem_owner_vbase[i] = local_mem_vbase[local_mem_owner_peer[i]];
}

void upcxx::finalize() {
  UPCXX_ASSERT_ALWAYS(backend::init_count > 0);
  
  if(0 != --backend::init_count)
    return;
  
  upcxx::barrier();
  
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

void* backend::localize_memory(intrank_t rank, uintptr_t raw) {
  UPCXX_ASSERT(
    local_peer_lb <= rank && rank < local_peer_ub,
    "Rank "<<rank<<" is not local with current rank ("<<upcxx::rank_me()<<")."
  );

  intrank_t peer = rank - local_peer_lb;
  uintptr_t u = raw + local_mem_local_minus_remote[peer];

  UPCXX_ASSERT(
    u - local_mem_vbase[peer] < local_mem_size[peer], // unsigned arithmetic handles both sides of the interval test
    "Memory address (raw="<<raw<<", local="<<reinterpret_cast<void*>(u)<<") is not within shared segment of rank "<<rank<<"."
  );

  return reinterpret_cast<void*>(u);
}

tuple<intrank_t/*rank*/, uintptr_t/*raw*/> backend::globalize_memory(void *addr) {
  intrank_t peer_n = local_peer_ub - local_peer_lb;
  uintptr_t uaddr = reinterpret_cast<uintptr_t>(addr);

  // key a pointer to one-passed the vbase less-or-equal to addr
  uintptr_t *key = std::upper_bound(
    local_mem_owner_vbase.get(),
    local_mem_owner_vbase.get() + peer_n,
    uaddr
  );

  int key_ix = key - local_mem_owner_vbase.get();

  #define bad_memory "Local memory "<<addr<<" is not in any local rank's shared segment."

  UPCXX_ASSERT(key_ix > 0, bad_memory);
  
  intrank_t peer = local_mem_owner_peer[key_ix-1];

  UPCXX_ASSERT(uaddr - local_mem_vbase[peer] <= local_mem_size[peer], bad_memory);
  
  return std::make_tuple(
    local_peer_lb + peer,
    uaddr - local_mem_local_minus_remote[peer]
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
    rma_get_cb(Fn fn): fn_{std::move(fn)} {}

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
    size_t buf_size,
    size_t buf_align
  ) {
  
  intrank_t rank_s = backend::rank_me;
  
  backend::send_am_persona<progress_level::internal>(
    rank_d,
    persona_d,
    [=]() {
      // TODO: Elide rma_get (copy) for node-local sends with pointer
      // translation and execution directly from source buffer.
      
      void *buf_d = upcxx::allocate(buf_size, buf_align);
      UPCXX_ASSERT_ALWAYS(buf_d != nullptr, "Exhausted shared segment!");
      
      rma_get(
        buf_d, rank_s, buf_s, buf_size,
        [=]() {
          // Notify source rank it can free buffer.
          gasnet::send_am_restricted(rank_s,
            [=]() { upcxx::deallocate(buf_s); }
          );
          
          backend::during_level<level>([=]() {
            // Execute buffer.
            parcel_reader r{buf_d};
            command_execute(r) >> [=]() {
              upcxx::deallocate(buf_d);
            };
          });
        }
      );
    }
  );
}

template void gasnet::send_am_rdzv<progress_level::internal>(intrank_t, persona*, void*, size_t, size_t);
template void gasnet::send_am_rdzv<progress_level::user>(intrank_t, persona*, void*, size_t, size_t);

void gasnet::after_gasnet() {
  if(detail::tl_progressing >= 0)
    return;
  detail::tl_progressing = (int)progress_level::internal;
  
  bool have_master = UPCXX_BACKEND_GASNET_SEQ || backend::master.active_with_caller();
  int total_exec_n = 0;
  int exec_n;
  
  do {
    exec_n = 0;
    
    if(have_master) {
      #if UPCXX_BACKEND_GASNET_SEQ
        exec_n += gasnet::master_hcbs.burst(4);
      #endif
      
      detail::persona_as_top(backend::master, [&]() {
        exec_n += rpcs_internal_.burst(20);
      });
    }
    
    detail::persona_foreach_active([&](persona &p) {
      #if UPCXX_BACKEND_GASNET_PAR
        exec_n += p.backend_state_.hcbs.burst(4);
      #endif
      exec_n += detail::persona_burst(p, progress_level::internal);
    });
    
    total_exec_n += exec_n;
  }
  while(total_exec_n < 100 && exec_n != 0);
  
  detail::tl_progressing = -1;
}


////////////////////////////////////////////////////////////////////////
// from: upcxx/backend.hpp

void upcxx::progress(progress_level level) {
  if(detail::tl_progressing >= 0)
    return;
  detail::tl_progressing = (int)level;
  
  bool have_master = backend::master.active_with_caller();
  int total_exec_n = 0;
  int exec_n;
  
  gasnet_AMPoll();
  
  do {
    exec_n = 0;
    
    if(have_master) {
      #if UPCXX_BACKEND_GASNET_SEQ
        exec_n += gasnet::master_hcbs.burst(4);
      #endif
      
      detail::persona_as_top(backend::master, [&]() {
        exec_n += rpcs_internal_.burst(100);
        if(level == progress_level::user)
          exec_n += rpcs_user_.burst(100);
      });
    }
    
    detail::persona_foreach_active([&](persona &p) {
      #if UPCXX_BACKEND_GASNET_PAR
        exec_n += p.backend_state_.hcbs.burst(4);
      #endif
      exec_n += detail::persona_burst(p, level);
    });
    
    total_exec_n += exec_n;
  }
  // Try really hard to do stuff before leaving attentiveness.
  while(total_exec_n < 1000 && exec_n != 0);
  
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
  thread_local int consecutive_nothings = 0;
  
  if(total_exec_n != 0)
    consecutive_nothings = 0;
  else if(++consecutive_nothings == 10) {
    sched_yield();
    consecutive_nothings = 0;
  }
  
  detail::tl_progressing = -1;
}

////////////////////////////////////////////////////////////////////////
// anonymous namespace

namespace {
  void am_eager_restricted(
      gex_Token_t,
      void *buf, size_t buf_size,
      gex_AM_Arg_t buf_align
    ) {
    
    future<> buf_done;
    
    if(0 == (reinterpret_cast<uintptr_t>(buf) & (buf_align-1))) {
      parcel_reader r{buf};
      buf_done = command_execute(r);
    }
    else {
      void *tmp;
      int ok = posix_memalign(&tmp, buf_align, buf_size);
      UPCXX_ASSERT_ALWAYS(ok == 0);
      
      std::memcpy((void**)tmp, (void**)buf, buf_size);
      
      parcel_reader r{tmp};
      buf_done = command_execute(r);
      
      std::free(tmp);
    }
    
    UPCXX_ASSERT(buf_done.ready());
  }
  
  void am_eager_master(
      gex_Token_t,
      void *buf, size_t buf_size,
      gex_AM_Arg_t buf_align_and_level
    ) {
    
    UPCXX_ASSERT(backend::rank_n!=-1);
    size_t buf_align = buf_align_and_level>>1;
    bool level_user = buf_align_and_level & 1;
    
    rpc_message *m = rpc_message::build_copy(buf, buf_size, buf_align);
    
    if(UPCXX_BACKEND_GASNET_PAR && !backend::master.active_with_caller()) {
      detail::persona_defer(
        backend::master,
        level_user ? progress_level::user : progress_level::internal,
        [=]() {
          m->execute_and_delete();
        }
      );
    }
    else {
      rpc_inbox &inbox = level_user ? rpcs_user_ : rpcs_internal_;
      inbox.enqueue(m);
    }
  }
  
  void am_eager_persona(
      gex_Token_t,
      void *buf, size_t buf_size,
      gex_AM_Arg_t buf_align_and_level,
      gex_AM_Arg_t per_lo,
      gex_AM_Arg_t per_hi
    ) {
    
    UPCXX_ASSERT(backend::rank_n!=-1);
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
    
    rpc_message *m = rpc_message::build_copy(buf, buf_size, buf_align);
    
    if(UPCXX_BACKEND_GASNET_PAR && (per != &backend::master || !per->active_with_caller())) {
      detail::persona_defer(
        *per,
        level_user ? progress_level::user : progress_level::internal,
        [=]() {
          m->execute_and_delete();
        }
      );
    }
    else {
      rpc_inbox &inbox = level_user ? rpcs_user_ : rpcs_internal_;
      inbox.enqueue(m);
    }
  }
}
