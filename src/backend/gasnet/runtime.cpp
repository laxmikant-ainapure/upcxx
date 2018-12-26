#include <upcxx/backend/gasnet/runtime.hpp>
#include <upcxx/backend/gasnet/runtime_internal.hpp>
#include <upcxx/backend/gasnet/upc_link.h>

#include <upcxx/os_env.hpp>
#include <upcxx/reduce.hpp>
#include <upcxx/team.hpp>

#include <algorithm>
#include <atomic>
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
using upcxx::team;
using upcxx::team_id;

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

static_assert(
  sizeof(gex_TM_t) == sizeof(uintptr_t),
  "Failed: sizeof(gex_TM_t) == sizeof(uintptr_t)"
);

////////////////////////////////////////////////////////////////////////
// from: upcxx/backend.hpp

int backend::init_count = 0;
  
intrank_t backend::rank_n = -1;
intrank_t backend::rank_me; // leave undefined so valgrind can catch it.

bool backend::verbose_noise = false;

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
  
  auto do_internal_progress = []() { upcxx::progress(progress_level::internal); };
  auto operation_cx_as_internal_future = upcxx::completions<upcxx::future_cx<upcxx::operation_cx_event, progress_level::internal>>{{}};
}

////////////////////////////////////////////////////////////////////////

namespace {
  // we statically allocate the top of the AM handler space, 
  // to improve interoperability with UPCR that uses the bottom
  #define UPCXX_NUM_AM_HANDLERS 4
  #define UPCXX_AM_INDEX_BASE   (256 - UPCXX_NUM_AM_HANDLERS)
  enum {
    id_am_eager_restricted = UPCXX_AM_INDEX_BASE,
    id_am_eager_master,
    id_am_eager_persona,
    id_am_bcast_master_eager,
    _id_am_endpost
  };
  static_assert(UPCXX_AM_INDEX_BASE >= GEX_AM_INDEX_BASE, "Incorrect UPCXX_AM_INDEX_BASE");
  static_assert((int)_id_am_endpost - UPCXX_AM_INDEX_BASE == UPCXX_NUM_AM_HANDLERS, "Incorrect UPCXX_NUM_AM_HANDLERS");
    
  void am_eager_restricted(gex_Token_t, void *buf, size_t buf_size, gex_AM_Arg_t buf_align);
  void am_eager_master(gex_Token_t, void *buf, size_t buf_size, gex_AM_Arg_t buf_align_and_level);
  void am_eager_persona(gex_Token_t, void *buf, size_t buf_size, gex_AM_Arg_t buf_align_and_level,
                        gex_AM_Arg_t persona_ptr_lo, gex_AM_Arg_t persona_ptr_hi);

  void am_bcast_master_eager(gex_Token_t, void *buf, size_t buf_size, gex_AM_Arg_t buf_align_and_level);
  
  #define AM_ENTRY(name, arg_n) \
    {id_##name, (void(*)())name, GEX_FLAG_AM_MEDIUM | GEX_FLAG_AM_REQUEST, arg_n, nullptr, #name}
  
  gex_AM_Entry_t am_table[] = {
    AM_ENTRY(am_eager_restricted, 1),
    AM_ENTRY(am_eager_master, 1),
    AM_ENTRY(am_eager_persona, 3),
    AM_ENTRY(am_bcast_master_eager, 1)
  };
}

////////////////////////////////////////////////////////////////////////
// shared heap management

#include <upcxx/dl_malloc.h>

namespace {
  gex_TM_t world_tm;
  gex_TM_t local_tm;

  std::mutex segment_lock_;
  intptr_t allocs_live_n_ = 0;
  mspace segment_mspace_;

  // scratch space for the local_team, if required
  size_t local_scratch_sz = 0;
  void  *local_scratch_ptr = nullptr;

  bool upcxx_use_upc_alloc = true;
  bool upcxx_upc_heap_coll = false;

  bool   shared_heap_isinit = false;
  void  *shared_heap_base = nullptr;
  size_t shared_heap_sz = 0;

  std::string format_memsize(size_t memsize) {
    char buf[80];
    return std::string(gasnett_format_number(memsize, buf, sizeof(buf), 1));
  }

  void heap_init_internal(size_t &size) {
    UPCXX_ASSERT_ALWAYS(!shared_heap_isinit);

    void *segment_base = 0;
    size_t segment_size = 0;
    int ok = gex_Segment_QueryBound(
        world_tm, backend::rank_me, &segment_base, nullptr, &segment_size
    );
    UPCXX_ASSERT_ALWAYS(ok == GASNET_OK);

    if (upcxx_upc_is_linked()) {
      static bool firstcall = true;
      if (firstcall) {
        firstcall = false;
        // UPCXX_USE_UPC_ALLOC enables the use of the UPC allocator to replace our allocator
        upcxx_use_upc_alloc = !!gasnett_getenv_yesno_withdefault("UPCXX_USE_UPC_ALLOC" , upcxx_use_upc_alloc);
        if (!upcxx_use_upc_alloc) {
          // UPCXX_UPC_HEAP_COLL: selects the use of the collective or non-collective UPC shared heap to host the UPC++ allocator
          upcxx_upc_heap_coll = !!gasnett_getenv_yesno_withdefault("UPCXX_UPC_HEAP_COLL" , upcxx_upc_heap_coll);
        }
      }
      if (local_scratch_sz && !local_scratch_ptr) { 
        // allocate local scratch separately from the heap to ensure it persists
        // we do this before segment allocation to prevent fragmentation issues
        local_scratch_ptr = upcxx_upc_all_alloc(local_scratch_sz);
      }
      if (upcxx_use_upc_alloc) {
        shared_heap_base = segment_base;
        size = segment_size;
      } else {
        shared_heap_base = upcxx_upc_alloc(size);
      }
    } else { // stand-alone UPC++
      upcxx_use_upc_alloc = false;
      size = segment_size;
      shared_heap_base = segment_base;
    }
    shared_heap_sz = size;

    if (!upcxx_use_upc_alloc) {
      // init dlmalloc to run over our piece of the segment
      segment_mspace_ = create_mspace_with_base(shared_heap_base, shared_heap_sz, 0);
      // ensure dlmalloc never tries to mmap anything from the system
      mspace_set_footprint_limit(segment_mspace_, shared_heap_sz);
    }
    if(backend::verbose_noise) {
      uint64_t maxsz = shared_heap_sz;
      uint64_t minsz = shared_heap_sz;
      gex_Event_Wait(gex_Coll_ReduceToOneNB(world_tm, 0, &maxsz, &maxsz, GEX_DT_U64, sizeof(maxsz), 1, GEX_OP_MAX, 0,0,0));
      gex_Event_Wait(gex_Coll_ReduceToOneNB(world_tm, 0, &minsz, &minsz, GEX_DT_U64, sizeof(minsz), 1, GEX_OP_MIN, 0,0,0));
      if (backend::rank_me == 0) {
        std::cerr
        <<std::string(70,'/')<<std::endl
        <<"heap_init_internal(): Shared heap statistics: \n"
        << "  max size: 0x" << std::hex << maxsz << " (" << format_memsize(maxsz) << ")\n"
        << "  min size: 0x" << std::hex << minsz << " (" << format_memsize(minsz) << ")\n"
        << "  P0 base:  " << shared_heap_base <<std::endl
        <<std::string(70,'/')<<std::endl;
      }
    }
    shared_heap_isinit = true;

    if (!upcxx_upc_is_linked()) {
      if (local_scratch_sz && !local_scratch_ptr) { 
        local_scratch_ptr = upcxx::allocate(local_scratch_sz, GASNET_PAGESIZE);
        UPCXX_ASSERT_ALWAYS(local_scratch_ptr);
      }
    }
  }
  void init_localheap_tables(void);
}

// WARNING: This is not a documented or supported entry point, and may soon be removed!!
// void upcxx::destroy_heap(void):
//
// Precondition: The shared heap is a live state, either by virtue
// of library initialization, or a prior call to upcxx::restore_heap.
// Calling thread must have the master persona.
//
// This collective call over all processes enforces an user-level entry barrier,
// and then destroys the entire shared heap. Behavior is undefined if any
// live objects remain in the shared heap at the time of destruction -
// this includes shared objects allocated directly by the application 
// (ie via upcxx::new* and upcxx::allocate* calls), and those allocated 
// indirectly on its behalf by the runtime. The list of library operations 
// that may indirectly allocate shared objects and their ensuing lifetime
// is implementation-defined.
//
// After this call, the shared heap of all processes are in a dead state.
// While dead, any calls to library functions that trigger shared object
// creation have undefined behavior. The list of such functions is
// implementation-defined.

void upcxx::destroy_heap(void) {
  UPCXX_ASSERT_ALWAYS(backend::master.active_with_caller());
  UPCXX_ASSERT_ALWAYS(shared_heap_isinit);
  backend::quiesce(upcxx::world(), entry_barrier::user);

  if (allocs_live_n_ > 0) {
     char warning[200];
     snprintf(warning, sizeof(warning), 
        "%i: WARNING: upcxx::destroy_heap() called with %lli live shared objects\n",
        backend::rank_me, (long long)allocs_live_n_);
     cerr << warning << flush;
  }
  if (upcxx_use_upc_alloc) { 
    if (!backend::rank_me) {
      cerr << "WARNING: upcxx::destroy_heap() is not supported for UPCXX_USE_UPC_ALLOC=yes" << endl;
    }
  } else {
    allocs_live_n_ = 0;

    destroy_mspace(segment_mspace_);
    segment_mspace_ = 0;

    if (upcxx_upc_is_linked()) {
      if (upcxx_upc_heap_coll) upcxx_upc_all_free(shared_heap_base);
      else upcxx_upc_free(shared_heap_base);
      shared_heap_base = nullptr;
    }
  }

  shared_heap_isinit = false;
}

// void upcxx::restore_heap(void):
//
// Precondition: The shared heap is a live state, either by virtue
// of library initialization, or a prior call to upcxx::restore_heap.
// Calling thread must have the master persona.
//
// This collective call over all processes re-initializes the shared heap of 
// all processes, returning them to a live state.

void upcxx::restore_heap(void) {
  UPCXX_ASSERT_ALWAYS(backend::master.active_with_caller());
  UPCXX_ASSERT_ALWAYS(!shared_heap_isinit);
  UPCXX_ASSERT_ALWAYS(shared_heap_sz > 0);

  if (upcxx_use_upc_alloc) {
    // unsupported/ignored
  } else { 
    heap_init_internal(shared_heap_sz);
    init_localheap_tables();
  }
  shared_heap_isinit = true;

  gasnet_barrier_notify(0, GASNET_BARRIERFLAG_ANONYMOUS);
  int ok = gasnet_barrier_wait(0, GASNET_BARRIERFLAG_ANONYMOUS);
  UPCXX_ASSERT_ALWAYS(ok == GASNET_OK);
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

  if (upcxx_upc_is_linked()) {
    upcxx_upc_init(&client, &endpoint, &world_tm);
  } else { 
    ok = gex_Client_Init(&client, &endpoint, &world_tm, "upcxx", nullptr, nullptr, 0);
    UPCXX_ASSERT_ALWAYS(ok == GASNET_OK);
  }

  backend::rank_n = gex_TM_QuerySize(world_tm);
  backend::rank_me = gex_TM_QueryRank(world_tm);
  
  // issue 100: hook up GASNet envvar services
  detail::getenv = ([](const char *key){ return gasnett_getenv(key); });
  detail::getenv_report = ([](const char *key, const char *val, bool is_dflt){ 
                                      gasnett_envstr_display(key,val,is_dflt); });
  
  backend::verbose_noise = os_env<bool>("UPCXX_VERBOSE", false);

  //////////////////////////////////////////////////////////////////////////////
  // UPCXX_SHARED_HEAP_SIZE environment handling

  // Determine a bound on the max usable shared segment size
  size_t gasnet_max_segsize = gasnet_getMaxLocalSegmentSize();
  if (upcxx_upc_is_linked()) {
    gasnet_max_segsize = gasnet_getMaxGlobalSegmentSize();
    size_t upc_segment_pad = 16*1024*1024; // TODO: replace this hack
    UPCXX_ASSERT_ALWAYS(gasnet_max_segsize > upc_segment_pad);
    gasnet_max_segsize -= upc_segment_pad;
  }

  const char *segment_keyname = "UPCXX_SHARED_HEAP_SIZE";
  { // Accept UPCXX_SEGMENT_MB for backwards compatibility:
    const char *old_keyname = "UPCXX_SEGMENT_MB";
    if (!detail::getenv(segment_keyname) && detail::getenv(old_keyname)) segment_keyname = old_keyname;
  }
  bool use_max = false;
  { // Accept m/MAX/i to request the largest available segment (limited by GASNET_MAX_SEGSIZE)
    const char *val = detail::getenv(segment_keyname);
    if (val) {
      std::string maxcheck = val;
      std::transform( maxcheck.begin(), maxcheck.end(), maxcheck.begin(),
                  [](unsigned char c) { return std::toupper(c); }); 
      use_max = (maxcheck == "MAX");
    }
  }

  size_t segment_size = 0;
  if (use_max) {
    segment_size = gasnet_max_segsize;
    gasnett_envint_display(segment_keyname, segment_size, 0, 1);
  } else {
    int64_t szval = os_env(segment_keyname, 128<<20, 1<<20); // default units = MB
    int64_t minheap = 2*GASNET_PAGESIZE; // space for local scratch and heap
    UPCXX_ASSERT_ALWAYS(szval >= minheap, segment_keyname << " too small!");
    segment_size = szval;
  }
  // page align: page size should always be a power of 2
  segment_size = (segment_size + GASNET_PAGESIZE-1) & -GASNET_PAGESIZE;

  // now adjust the segment size if it's less than the GASNET_MAX_SEGSIZE
  if (segment_size > gasnet_max_segsize) {
    if(upcxx::rank_me() == 0) {
      cerr << "WARNING: Requested UPC++ shared heap size (" << format_memsize(segment_size) << ") "
              "is larger than the GASNet segment size (" << format_memsize(gasnet_max_segsize) << "). "
              "Adjusted shared heap size to " << format_memsize(gasnet_max_segsize) << ".\n";
    }
    segment_size = gasnet_max_segsize;
  }
 
  // ready master persona
  backend::initial_master_scope = new persona_scope(backend::master);
  UPCXX_ASSERT_ALWAYS(backend::master.active_with_caller());
  
  // Build team upcxx::world()
  ::new(&detail::the_world_team.raw) upcxx::team(
    detail::internal_only(),
    backend::team_base{reinterpret_cast<uintptr_t>(world_tm)},
    digest{0x1111111111111111, 0x1111111111111111},
    backend::rank_n, backend::rank_me
  );
  
  // Create the GEX segment
  if (upcxx_upc_is_linked()) {
    if (!backend::rank_me) 
      cerr << "UPC++ BETA FEATURE NOTICE: Activating interoperability support for the Berkeley UPC Runtime. "
              "This is not an officially supported feature." << endl;
  } else {
    ok = gex_Segment_Attach(&segment, world_tm, segment_size);
    UPCXX_ASSERT_ALWAYS(ok == GASNET_OK);
  }

  // AM handler registration
  ok = gex_EP_RegisterHandlers(endpoint, am_table, sizeof(am_table)/sizeof(am_table[0]));
  UPCXX_ASSERT_ALWAYS(ok == GASNET_OK);

  size_t am_medium_size = gex_AM_MaxRequestMedium(
    world_tm,
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
  
  //////////////////////////////////////////////////////////////////////////////
  // Setup the local-memory neighborhood tables.
  
  gex_RankInfo_t *nbhd;
  gex_Rank_t peer_n, peer_me;
  gex_System_QueryNbrhdInfo(&nbhd, &peer_n, &peer_me);

  bool contiguous_nbhd = true;
  for(gex_Rank_t p=1; p < peer_n; p++)
    contiguous_nbhd &= (nbhd[p].gex_jobrank == 1 + nbhd[p-1].gex_jobrank);

  bool const local_is_world = ((intrank_t)peer_n == backend::rank_n);

  if (!local_is_world) { // determine (upper bound on) scratch requirements for local_team
    local_scratch_sz = gex_TM_Split(
      &local_tm, world_tm,
      /*color*/nbhd[0].gex_jobrank, /*key*/peer_me,
      nullptr, 0,
      peer_n == 1
        ? GEX_FLAG_TM_SCRATCH_SIZE_MIN
        : GEX_FLAG_TM_SCRATCH_SIZE_RECOMMENDED
    );
  }

  // setup shared segment allocator
  heap_init_internal(segment_size);
  
  if (local_is_world) {
    if(backend::verbose_noise && backend::rank_me == 0) {
      std::cerr
        <<std::string(70,'/')<<std::endl
        <<"upcxx::init(): Whole world is in same local team."<<std::endl
        <<std::string(70,'/')<<std::endl;
    }
    
    backend::pshm_peer_lb = 0;
    backend::pshm_peer_ub = backend::rank_n;
    peer_n = backend::rank_n;
    peer_me = backend::rank_me;
    local_tm = world_tm;
  } else { // !local_is_world
    if(!contiguous_nbhd) {
      // Discontiguous rank-set is collapsed to singleton set of "me"
      backend::pshm_peer_lb = backend::rank_me;
      backend::pshm_peer_ub = backend::rank_me + 1;
      peer_n = 1;
      peer_me = 0;
    }
    else {
      // True subset local team
      backend::pshm_peer_lb = nbhd[0].gex_jobrank;
      backend::pshm_peer_ub = nbhd[0].gex_jobrank + peer_n;
    }
    
    if(backend::verbose_noise) {
      struct local_team_stats {
        int count;
        int min_size, max_size;
      };
      
      local_team_stats stats = {peer_me == 0 ? 1 : 0, (int)peer_n, (int)peer_n};
      
      gex_Event_Wait(gex_Coll_ReduceToOneNB(
          world_tm, 0,
          &stats, &stats,
          GEX_DT_USER, sizeof(local_team_stats), 1,
          GEX_OP_USER,
          (gex_Coll_ReduceFn_t)[](const void *arg1, void *arg2_out, std::size_t n, const void*) {
            const auto *in = (local_team_stats*)arg1;
            auto *acc = (local_team_stats*)arg2_out;
            for(std::size_t i=0; i != n; i++) {
              acc[i].count += in[i].count;
              acc[i].min_size = std::min(acc[i].min_size, in[i].min_size);
              acc[i].max_size = std::max(acc[i].max_size, in[i].max_size);
            }
          },
          nullptr, 0
        )
      );
      
      if(backend::rank_me == 0) {
        std::cerr
          <<std::string(70,'/')<<std::endl
          <<"upcxx::init(): local team statistics:"<<std::endl
          <<"  local teams = "<<stats.count<<std::endl
          <<"  min rank_n = "<<stats.min_size<<std::endl
          <<"  max rank_n = "<<stats.max_size<<std::endl;
        if(stats.count == backend::rank_n) {
          std::cerr<<"  WARNING: All local team's are singletons. Memory sharing between ranks will never succeed."<<std::endl;
        }
        std::cerr<<std::string(70,'/')<<std::endl;
      }
    }
    
    UPCXX_ASSERT_ALWAYS( local_scratch_sz && local_scratch_ptr );
    
    gex_TM_Split(
      &local_tm, world_tm,
      /*color*/backend::pshm_peer_lb, /*key*/peer_me,
      local_scratch_ptr, local_scratch_sz,
      /*flags*/0
    );
    
    if (!upcxx_upc_is_linked()) 
      gex_TM_SetCData(local_tm, local_scratch_ptr );
  }
  
  backend::pshm_peer_n = peer_n;
  
  // Build upcxx::local_team()
  ::new(&detail::the_local_team.raw) upcxx::team(
    detail::internal_only(),
    backend::team_base{reinterpret_cast<uintptr_t>(local_tm)},
    // we use different digests even if local_tm==world_tm
    digest{0x2222222222222222, 0x2222222222222222},
    peer_n, peer_me
  );
  
  // Setup local peer address translation tables
  init_localheap_tables();

  //////////////////////////////////////////////////////////////////////////////
  // Exit barrier
  
  gasnet_barrier_notify(0, GASNET_BARRIERFLAG_ANONYMOUS);
  ok = gasnet_barrier_wait(0, GASNET_BARRIERFLAG_ANONYMOUS);
  UPCXX_ASSERT_ALWAYS(ok == GASNET_OK);
}

namespace {
 void init_localheap_tables(void) {
  const gex_Rank_t peer_n = backend::pshm_peer_n;

  backend::pshm_local_minus_remote.reset(new uintptr_t[peer_n]);
  backend::pshm_vbase.reset(new uintptr_t[peer_n]);
  backend::pshm_size.reset(new uintptr_t[peer_n]);
  pshm_owner_vbase.reset(new uintptr_t[peer_n]);
  pshm_owner_peer.reset(new intrank_t[peer_n]);
  
  for(gex_Rank_t p=0; p < peer_n; p++) {
    char *owner_vbase, *local_vbase;
    void *owner_vbase_vp, *local_vbase_vp;
    uintptr_t size;

    gex_Segment_QueryBound(
      /*team*/world_tm,
      /*rank*/backend::pshm_peer_lb + p,
      &owner_vbase_vp, 
      &local_vbase_vp, 
      &size
    );
    owner_vbase = reinterpret_cast<char*>(owner_vbase_vp);
    local_vbase = reinterpret_cast<char*>(local_vbase_vp);
    UPCXX_ASSERT_ALWAYS(owner_vbase && local_vbase && size);

    if (upcxx_upc_is_linked() && !upcxx_use_upc_alloc) { 
    #if UPCXX_STRICT_SEGMENT // this logic prevents UPCR shared objects from passing upcxx::try_global_ptr
      // We have the GEX segment info for the local peer, but
      // the UPC++ shared heap is a subset of the GEX segment.
      // Determine the necessary adjustment to locate our shared heap:
      std::pair<uintptr_t, uintptr_t> info(
        reinterpret_cast<char *>(shared_heap_base) - owner_vbase, // base offset on owner
        shared_heap_sz // size
      );
      gex_Event_Wait(gex_Coll_BroadcastNB( local_tm, p, &info, &info, sizeof(info), 0));

      UPCXX_ASSERT_ALWAYS(info.first < size);
      owner_vbase += info.first;
      local_vbase += info.first;
      UPCXX_ASSERT_ALWAYS(info.second <= size);
      size = info.second;
    #endif
    }
    
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
 }
}

void upcxx::finalize() {
  UPCXX_ASSERT_ALWAYS(backend::master.active_with_caller());
  UPCXX_ASSERT_ALWAYS(backend::init_count > 0);
  
  if(0 != --backend::init_count)
    return;
  
  { // barrier
    gasnet_barrier_notify(0, GASNET_BARRIERFLAG_ANONYMOUS);
    while(GASNET_OK != gasnet_barrier_try(0, GASNET_BARRIERFLAG_ANONYMOUS))
      upcxx::progress();
  }
  
  struct popn_stats_t {
    int64_t sum, min, max;
  };
  
  auto reduce_popn_to_rank0 = [](int64_t arg)->popn_stats_t {
    // We could use `gex_Coll_ReduceToOne`, but this gives us a test of reductions
    // using our "internal only" completions.
    return upcxx::reduce_one(
        popn_stats_t{arg, arg, arg},
        [](popn_stats_t a, popn_stats_t b)->popn_stats_t {
          return {a.sum + b.sum, std::min(a.min, b.min), std::max(a.max, b.max)};
        },
        /*root=*/0, upcxx::world(),
        operation_cx_as_internal_future
      ).wait(do_internal_progress);
  };
  
  if(backend::verbose_noise) {
    int64_t objs_local = detail::registry.size() - 2; // minus `world` and `local_team`
    popn_stats_t objs = reduce_popn_to_rank0(objs_local);
    
    if(backend::rank_me == 0 && objs.sum != 0) {
      std::cerr
        <<std::string(70,'/')<<std::endl
        <<"upcxx::finalize(): Objects remain within registries at finalize"<<std::endl
        <<"(could be teams, dist_object's, or outstanding collectives)."<<std::endl
        <<"  total = "<<objs.sum<<std::endl
        <<"  per rank min = "<<objs.min<<std::endl
        <<"  per rank max = "<<objs.max<<std::endl
        <<std::string(70,'/')<<std::endl;
    }
  }
  
  if(backend::verbose_noise) {
    int64_t live_local = allocs_live_n_;
    
    if(gasnet::handle_of(detail::the_local_team.value) !=
       gasnet::handle_of(detail::the_world_team.value)
       && !upcxx_upc_is_linked())
       live_local -= 1; // minus local_team scratch
    
    popn_stats_t live = reduce_popn_to_rank0(live_local);
    
    if(backend::rank_me == 0 && live.sum != 0) {
      std::cerr
        <<std::string(70,'/')<<std::endl
        <<"upcxx::finalize(): Shared segment allocations live at finalize:"<<std::endl
        <<"  total = "<<live.sum<<std::endl
        <<"  per rank min = "<<live.min<<std::endl
        <<"  per rank max = "<<live.max<<std::endl
        <<std::string(70,'/')<<std::endl;
    }
  }
  
  { // Tear down local_team
    if(gasnet::handle_of(detail::the_local_team.value) !=
       gasnet::handle_of(detail::the_world_team.value))
      {/*TODO: add local team destruct once GEX has the API.*/}
    
    detail::the_local_team.value.destroy();
    detail::the_local_team.destruct();
  }
  
  // can't just destroy world, it needs special attention
  detail::registry.erase(detail::the_world_team.value.id().dig_);
  
  if(backend::initial_master_scope != nullptr)
    delete backend::initial_master_scope;
}

void upcxx::liberate_master_persona() {
  UPCXX_ASSERT_ALWAYS(&upcxx::current_persona() == &backend::master);
  UPCXX_ASSERT_ALWAYS(backend::initial_master_scope != nullptr);
  
  delete backend::initial_master_scope;
  
  backend::initial_master_scope = nullptr;
}

void* upcxx::allocate(size_t size, size_t alignment) {
  UPCXX_ASSERT(shared_heap_isinit);
  #if UPCXX_BACKEND_GASNET_SEQ
    UPCXX_ASSERT(backend::master.active_with_caller());
  #elif UPCXX_BACKEND_GASNET_PAR
    std::lock_guard<std::mutex> locked{segment_lock_};
  #endif
  
  void *p;
  if (upcxx_use_upc_alloc) {
    // must overallocate and pad to ensure alignment
    UPCXX_ASSERT(alignment < 1U<<31);
    alignment = std::max(alignment, (size_t)4);
    UPCXX_ASSERT((alignment & (alignment-1)) == 0); // assumed to be power-of-two
    uintptr_t base = (uintptr_t)upcxx_upc_alloc(size+alignment);
    uintptr_t user = (base+alignment) & ~(alignment-1);
    uintptr_t pad = (user - base);
    UPCXX_ASSERT(pad >= 4 && pad <= alignment);
    *(reinterpret_cast<uint32_t*>(user-4)) = pad; // store padding amount in header
    p = reinterpret_cast<void *>(user);
  } else {
    p = mspace_memalign(segment_mspace_, alignment, size);
  }
  if_pt(p) allocs_live_n_ += 1;
  //UPCXX_ASSERT(p != nullptr);
  UPCXX_ASSERT(reinterpret_cast<uintptr_t>(p) % alignment == 0);
  return p;
}

void upcxx::deallocate(void *p) {
  UPCXX_ASSERT(shared_heap_isinit);
  #if UPCXX_BACKEND_GASNET_SEQ
    UPCXX_ASSERT(backend::master.active_with_caller());
  #elif UPCXX_BACKEND_GASNET_PAR
    std::lock_guard<std::mutex> locked{segment_lock_};
  #endif
  if_pf (!p) return;
  
  if (upcxx_use_upc_alloc) {
    // parse alignment header to recover original base ptr
    uintptr_t user = reinterpret_cast<uintptr_t>(p);
    UPCXX_ASSERT((user & 0x3) == 0);
    uint32_t  pad = *(reinterpret_cast<uint32_t*>(user-4));
    uintptr_t base = user-pad;
    upcxx_upc_free(reinterpret_cast<void *>(base));
  } else {
    mspace_free(segment_mspace_, p);
  }
  allocs_live_n_ -= 1;
}

//////////////////////////////////////////////////////////////////////
// from: upcxx/backend.hpp

void backend::quiesce(team &tm, upcxx::entry_barrier eb) {
  switch(eb) {
  case entry_barrier::none:
    break;
  case entry_barrier::internal:
  case entry_barrier::user: {
      // memory fencing is handled inside gex_Coll_BarrierNB + gex_Event_Test
      //std::atomic_thread_fence(std::memory_order_release);
      
      gex_Event_t e = gex_Coll_BarrierNB( gasnet::handle_of(tm), 0);
      
      while(0 != gex_Event_Test(e)) {
        upcxx::progress(
          eb == entry_barrier::internal
            ? progress_level::internal
            : progress_level::user
        );
      }
      
      //std::atomic_thread_fence(std::memory_order_acquire);
    } break;
  }
}

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

tuple<intrank_t/*rank*/, uintptr_t/*raw*/>  backend::globalize_memory(
    void const *addr,
    tuple<intrank_t/*rank*/, uintptr_t/*raw*/> otherwise
  ) {
  intrank_t peer_n = pshm_peer_ub - pshm_peer_lb;
  uintptr_t uaddr = reinterpret_cast<uintptr_t>(addr);

  // key is a pointer to one past the last vbase less-or-equal to addr.
  uintptr_t *key = std::upper_bound(
      pshm_owner_vbase.get(),
      pshm_owner_vbase.get() + peer_n,
      uaddr
    );

  int key_ix = key - pshm_owner_vbase.get();
  
  if(key_ix <= 0)
    return otherwise;
  
  intrank_t peer = pshm_owner_peer[key_ix-1];

  if(uaddr - pshm_vbase[peer] <= pshm_size[peer])
    return std::make_tuple(
        pshm_peer_lb + peer,
        uaddr - pshm_local_minus_remote[peer]
      );
  else
    return otherwise;
}

intrank_t backend::team_rank_from_world(team &tm, intrank_t rank) {
  gex_Rank_t got = gex_TM_TranslateJobrankToRank(gasnet::handle_of(tm), rank);
  UPCXX_ASSERT(got != GEX_RANK_INVALID);
  return got;
}

intrank_t backend::team_rank_from_world(team &tm, intrank_t rank, intrank_t otherwise) {
  gex_Rank_t got = gex_TM_TranslateJobrankToRank(gasnet::handle_of(tm), rank);
  return got == GEX_RANK_INVALID ? otherwise : (intrank_t)got;
}

intrank_t backend::team_rank_to_world(team &tm, intrank_t peer) {
  return gex_TM_TranslateRankToJobrank(gasnet::handle_of(tm), peer);
}

////////////////////////////////////////////////////////////////////////
// from: upcxx/backend/gasnet/runtime.hpp

void gasnet::send_am_eager_restricted(
    team &tm,
    intrank_t recipient,
    void *buf,
    std::size_t buf_size,
    std::size_t buf_align
  ) {
  
  gex_AM_RequestMedium1(
    handle_of(tm), recipient,
    id_am_eager_restricted, buf, buf_size,
    GEX_EVENT_NOW, /*flags*/0,
    buf_align
  );
  
  after_gasnet();
}

void gasnet::send_am_eager_master(
    progress_level level,
    team &tm,
    intrank_t recipient,
    void *buf,
    std::size_t buf_size,
    std::size_t buf_align
  ) {
  
  gex_AM_RequestMedium1(
    handle_of(tm), recipient,
    id_am_eager_master, buf, buf_size,
    GEX_EVENT_NOW, /*flags*/0,
    buf_align<<1 | (level == progress_level::user ? 1 : 0)
  );
  
  after_gasnet();
}

void gasnet::send_am_eager_persona(
    progress_level level,
    team &tm,
    intrank_t recipient_rank,
    persona *recipient_persona,
    void *buf,
    std::size_t buf_size,
    std::size_t buf_align
  ) {
  
  gex_AM_Arg_t per_lo = reinterpret_cast<intptr_t>(recipient_persona) & 0xffffffffu;
  gex_AM_Arg_t per_hi = reinterpret_cast<intptr_t>(recipient_persona) >> 31 >> 1;

  gex_AM_RequestMedium3(
    handle_of(tm), recipient_rank,
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
      gasnet::handle_of(upcxx::world()),
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
    team &tm,
    intrank_t rank_d,
    persona *persona_d,
    void *buf_s,
    size_t cmd_size,
    size_t cmd_align
  ) {
  
  intrank_t rank_s = backend::rank_me;
  
  backend::send_am_persona<progress_level::internal>(
    tm, rank_d, persona_d,
    [=]() {
      if(backend::rank_is_local(rank_s)) {
        void *payload = backend::localize_memory_nonnull(rank_s, reinterpret_cast<std::uintptr_t>(buf_s));
        
        rpc_as_lpc *m = new rpc_as_lpc;
        m->payload = payload;
        m->the_vtbl.execute_and_delete = command<detail::lpc_base*>::get_executor(rpc_as_lpc::reader_of(m));
        m->vtbl = &m->the_vtbl;
        m->is_rdzv = true;
        m->rdzv_rank_s = rank_s;
        m->rdzv_rank_s_local = true;
        
        auto &tls = detail::the_persona_tls;
        tls.enqueue(*tls.get_top_persona(), level, m, /*known_active=*/std::true_type());
      }
      else {
        rpc_as_lpc *m = rpc_as_lpc::build_rdzv_lz(cmd_size, cmd_align);
        m->rdzv_rank_s = rank_s;
        m->rdzv_rank_s_local = false;
        
        rma_get(
          m->payload, rank_s, buf_s, cmd_size,
          [=]() {
            auto &tls = detail::the_persona_tls;
            int rank_s = m->rdzv_rank_s;
            
            m->the_vtbl.execute_and_delete = command<detail::lpc_base*>::get_executor(rpc_as_lpc::reader_of(m));
            tls.enqueue(*tls.get_top_persona(), level, m, /*known_active=*/std::true_type());
            
            // Notify source rank it can free buffer.
            gasnet::send_am_restricted(
              upcxx::world(), rank_s,
              [=]() { upcxx::deallocate(buf_s); }
            );
          }
        );
      }
    }
  );
}

template void gasnet::send_am_rdzv<progress_level::internal>(team&, intrank_t, persona*, void*, size_t, size_t);
template void gasnet::send_am_rdzv<progress_level::user>(team&, intrank_t, persona*, void*, size_t, size_t);

void gasnet::bcast_am_master_eager(
    progress_level level,
    upcxx::team &tm,
    intrank_t rank_d_ub, // in range [0, 2*rank_n-1)
    void *payload,
    size_t cmd_size, size_t cmd_align
  ) {
  
  intrank_t rank_me = tm.rank_me();
  intrank_t rank_n = tm.rank_n();
  
  gex_TM_t tm_gex = handle_of(tm);
  
  parcel_writer w(payload);
  team_id *p_tm_id = w.place_trivial_aligned<team_id>();
  intrank_t *p_sub_ub = w.place_trivial_aligned<intrank_t>();
  
  UPCXX_ASSERT(*p_tm_id == tm.id());
  if(p_tm_id) {/*silence "unused" warning*/}
  
  // loop over targets
  while(true) {
    intrank_t rank_d_mid = rank_me + 15*int64_t(rank_d_ub - rank_me)/16;
    
    // Send-to-self is stop condition.
    if(rank_d_mid == rank_me)
      break;
    
    intrank_t translate = rank_n <= rank_d_mid ? rank_n : 0;
    
    // Sub-interval bounds. Lower must be in [0,rank_n).
    intrank_t sub_lb = rank_d_mid - translate;
    intrank_t sub_ub = rank_d_ub - translate;
    
    *p_sub_ub = sub_ub;
    gex_AM_RequestMedium1(
      tm_gex, sub_lb,
      id_am_bcast_master_eager, payload, cmd_size,
      GEX_EVENT_NOW, /*flags*/0,
      cmd_align<<1 | (level == progress_level::user ? 1 : 0)
    );
    
    rank_d_ub = rank_d_mid;
  }
  
  gasnet::after_gasnet();
}

template<progress_level level>
int gasnet::bcast_am_master_rdzv(
    upcxx::team &tm,
    intrank_t rank_d_ub, // in range [0, 2*rank_n-1)
    intrank_t wrank_owner, // self or a local peer (in world)
    void *payload_sender, // in my address space
    std::atomic<int64_t> *refs_sender, // in my address space
    size_t cmd_size,
    size_t cmd_align
  ) {
  
  intrank_t rank_n = tm.rank_n();
  intrank_t rank_me = tm.rank_me();
  intrank_t wrank_sender = backend::rank_me;
  void *payload_owner = reinterpret_cast<void*>(
      backend::globalize_memory_nonnull(wrank_owner, payload_sender)
    );
  std::atomic<int64_t> *refs_owner = reinterpret_cast<std::atomic<int64_t>*>(
      backend::globalize_memory_nonnull(wrank_owner, refs_sender)
    );
  
  int refs_added = 0;
  
  // loop over targets
  while(true) {
    intrank_t rank_d_mid = rank_me + (rank_d_ub - rank_me)/2;
    
    // Send-to-self is stop condition.
    if(rank_d_mid == rank_me)
      break;
    
    intrank_t translate = rank_n <= rank_d_mid ? rank_n : 0;
    
    // Sub-interval bounds. Lower must be in [0,rank_n).
    intrank_t sub_lb = rank_d_mid - translate;
    intrank_t sub_ub = rank_d_ub - translate;
    
    refs_added += 1;
    
    backend::send_am_master<progress_level::internal>(
      tm, sub_lb,
      [=]() {
        if(backend::rank_is_local(wrank_sender)) {
          void *payload_target = backend::localize_memory_nonnull(wrank_owner, reinterpret_cast<std::uintptr_t>(payload_owner));
          std::atomic<int64_t> *refs_target = (std::atomic<int64_t>*)backend::localize_memory_nonnull(wrank_owner, reinterpret_cast<std::uintptr_t>(refs_owner));
          
          parcel_reader r(payload_target);
          team_id tm_id = r.pop_trivial_aligned<team_id>();
          /*ignore*/r.pop_trivial_aligned<intrank_t>();
          
          bcast_as_lpc *m = new bcast_as_lpc;
          m->the_vtbl.execute_and_delete = command<detail::lpc_base*>::get_executor(r);
          m->payload = payload_target;
          m->vtbl = &m->the_vtbl;
          m->is_rdzv = true;
          m->rdzv_rank_s = wrank_owner;
          m->rdzv_rank_s_local = true;
          m->rdzv_refs_s = refs_target;
          
          auto &tls = detail::the_persona_tls;
          tls.enqueue(*tls.get_top_persona(), level, m, /*known_active=*/std::true_type());
          
          int refs_added = bcast_am_master_rdzv<level>(
              tm_id.here(), sub_ub,
              wrank_owner, payload_target, refs_target,
              cmd_size, cmd_align
            );
          
          int64_t refs_now = refs_target->fetch_add(refs_added, std::memory_order_relaxed);
          refs_now += refs_added;
          // can't be done because enqueued rpc couldn't have run. (The `if(...)`
          // prevents "unused variable" warnings when asserts are off.)
          if(refs_now == 0) UPCXX_ASSERT(refs_now != 0);
        }
        else {
          bcast_as_lpc *m = rpc_as_lpc::build_rdzv_lz<bcast_as_lpc>(cmd_size, cmd_align);
          m->rdzv_rank_s = wrank_owner;
          m->rdzv_rank_s_local = false;
          m->rdzv_refs_s = refs_owner;
          
          rma_get(
            m->payload, wrank_owner, payload_owner, cmd_size,
            [=]() {
              intrank_t wrank_owner = m->rdzv_rank_s;
              
              std::atomic<int64_t> *refs_owner = m->rdzv_refs_s;
              m->rdzv_refs_s = &m->rdzv_refs_here;
              
              parcel_reader r(m->payload);
              team_id tm_id = r.pop_trivial_aligned<team_id>();
              /*ignore*/r.pop_trivial_aligned<intrank_t>();
              
              auto &tls = detail::the_persona_tls;
              m->the_vtbl.execute_and_delete = command<detail::lpc_base*>::get_executor(r);
              UPCXX_ASSERT(&backend::master == tls.get_top_persona());
              tls.enqueue(backend::master, level, m, /*known_active=*/std::true_type());
              
              m->rdzv_refs_here.store(1 + (1<<30), std::memory_order_relaxed);
              
              int refs_added = bcast_am_master_rdzv<level>(
                  tm_id.here(), sub_ub,
                  backend::rank_me, m->payload, &m->rdzv_refs_here,
                  cmd_size, cmd_align
                );
              
              int64_t refs_now = m->rdzv_refs_here.fetch_add(refs_added - (1<<30), std::memory_order_relaxed);
              refs_now += refs_added - (1<<30);
              // enqueued rpc shouldn't have run yet
              if(refs_now == 0) UPCXX_ASSERT(refs_now != 0);
              
              // Notify source rank it can free buffer.
              send_am_restricted(
                upcxx::world(), wrank_owner,
                [=]() {
                  if(0 == -1 + refs_owner->fetch_add(-1, std::memory_order_acq_rel))
                    upcxx::deallocate(payload_owner);
                }
              );
            }
          );
        }
      }
    );
    
    rank_d_ub = rank_d_mid;
  }
  
  return refs_added;
}

template int gasnet::bcast_am_master_rdzv<progress_level::internal>(
  upcxx::team&, intrank_t, intrank_t, void*, std::atomic<int64_t>*, size_t, size_t
);
template int gasnet::bcast_am_master_rdzv<progress_level::user>(
  upcxx::team&, intrank_t, intrank_t, void*, std::atomic<int64_t>*, size_t, size_t
);

namespace upcxx {
namespace backend {
namespace gasnet {
  template<>
  void rpc_as_lpc::cleanup</*never_rdzv=*/false>(detail::lpc_base *me1) {
    rpc_as_lpc *me = static_cast<rpc_as_lpc*>(me1);
    
    if(!me->is_rdzv)
      std::free(me->payload);
    else {
      if(me->rdzv_rank_s_local) {
        // Notify source rank it can free buffer.
        void *buf_s = reinterpret_cast<void*>(
            backend::globalize_memory_nonnull(me->rdzv_rank_s, me->payload)
          );
         
        send_am_restricted(
          upcxx::world(), me->rdzv_rank_s,
          [=]() { upcxx::deallocate(buf_s); }
        );
        
        delete me;
      }
      else {
        upcxx::deallocate(me->payload);
      }
    }
  }
  
  template<>
  void bcast_as_lpc::cleanup</*never_rdzv=*/false>(detail::lpc_base *me1) {
    bcast_as_lpc *me = static_cast<bcast_as_lpc*>(me1);
    
    if(!me->is_rdzv) {
      if(0 == --me->eager_refs)
        std::free(me->payload);
    }
    else {
      int64_t refs_now = -1 + me->rdzv_refs_s->fetch_add(-1, std::memory_order_acq_rel);
      
      if(me->rdzv_rank_s_local) {
        if(0 == refs_now) {
          // Notify source rank it can free buffer.
          void *buf_s = reinterpret_cast<void*>(
              backend::globalize_memory_nonnull(me->rdzv_rank_s, me->payload)
            );
          
          send_am_restricted(
            upcxx::world(), me->rdzv_rank_s,
            [=]() { upcxx::deallocate(buf_s); }
          );
        }
        
        delete me;
      }
      else {
        if(0 == refs_now)
          upcxx::deallocate(me->payload);
      }
    }
  }
}}}

template<typename RpcAsLpc>
RpcAsLpc* rpc_as_lpc::build_eager(
    void *cmd_buf,
    std::size_t cmd_size,
    std::size_t cmd_alignment
  ) {
  
  std::size_t msg_size = cmd_size;
  msg_size = (msg_size + alignof(RpcAsLpc)-1) & -alignof(RpcAsLpc);
  
  std::size_t msg_offset = msg_size;
  msg_size += sizeof(RpcAsLpc);
  
  void *msg_buf;
  int ok = posix_memalign(&msg_buf, cmd_alignment, msg_size);
  UPCXX_ASSERT_ALWAYS(ok == 0);
  
  // The (void**) casts *might* inform memcpy that it can assume word
  // alignment.
  std::memcpy((void**)msg_buf, (void**)cmd_buf, cmd_size);
  
  RpcAsLpc *m = ::new((char*)msg_buf + msg_offset) RpcAsLpc;
  m->payload = msg_buf;
  m->the_vtbl.execute_and_delete = command<detail::lpc_base*>::get_executor(RpcAsLpc::reader_of(m));
  m->vtbl = &m->the_vtbl;
  m->is_rdzv = false;
  
  return m;
}

template<typename RpcAsLpc>
RpcAsLpc* rpc_as_lpc::build_rdzv_lz(
    std::size_t cmd_size,
    std::size_t cmd_alignment // alignment requirement of packing
  ) {
  std::size_t offset = (cmd_size + alignof(RpcAsLpc)-1) & -alignof(RpcAsLpc);
  std::size_t buf_size = offset + sizeof(RpcAsLpc);
  std::size_t buf_align = std::max(cmd_alignment, alignof(RpcAsLpc));
  
  void *buf = upcxx::allocate(buf_size, buf_align);
  UPCXX_ASSERT_ALWAYS(buf != nullptr);
  
  RpcAsLpc *m = ::new((char*)buf + offset) RpcAsLpc;
  m->the_vtbl.execute_and_delete = nullptr; // filled in when GET completes
  m->vtbl = &m->the_vtbl;
  m->payload = buf;
  m->is_rdzv = true;
  
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
  
  #if GASNET_CONDUIT_SMP || GASNET_CONDUIT_UDP
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
      command<void*>::get_executor(parcel_reader(buf))(buf);
    }
    else {
      void *tmp;
      int ok = posix_memalign(&tmp, buf_align, buf_size);
      UPCXX_ASSERT_ALWAYS(ok == 0);
      
      std::memcpy((void**)tmp, (void**)buf, buf_size);
      
      command<void*>::get_executor(parcel_reader(buf))(buf);
      
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
    
    rpc_as_lpc *m = rpc_as_lpc::build_eager(buf, buf_size, buf_align);
    
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
    
    rpc_as_lpc *m = rpc_as_lpc::build_eager(buf, buf_size, buf_align);
    
    detail::persona_tls &tls = detail::the_persona_tls;
    
    tls.enqueue(
      *per,
      level_user ? progress_level::user : progress_level::internal,
      m,
      /*known_active=*/std::integral_constant<bool, !UPCXX_BACKEND_GASNET_PAR>()
    );
  }
  
  void am_bcast_master_eager(
      gex_Token_t,
      void *buf, size_t buf_size,
      gex_AM_Arg_t buf_align_and_level
    ) {
    using gasnet::bcast_as_lpc;
    
    size_t buf_align = buf_align_and_level>>1;
    bool level_user = buf_align_and_level & 1;
    progress_level level = level_user ? progress_level::user : progress_level::internal;
    
    bcast_as_lpc *m = rpc_as_lpc::build_eager<bcast_as_lpc>(buf, buf_size, buf_align);
    m->eager_refs = 2;
    
    detail::persona_tls &tls = detail::the_persona_tls;
    
    constexpr auto known_active = std::integral_constant<bool, !UPCXX_BACKEND_GASNET_PAR>();
    
    tls.defer(
      backend::master,
      progress_level::internal,
      [=]() {
        parcel_reader r(m->payload);
        team_id tm_id = r.pop_trivial_aligned<team_id>();
        intrank_t rank_d_ub = r.pop_trivial_aligned<intrank_t>();
        
        gasnet::bcast_am_master_eager(level, tm_id.here(), rank_d_ub, m->payload, buf_size, buf_align);
        
        if(0 == --m->eager_refs)
          std::free(m->payload);
      },
      known_active
    );
    
    tls.enqueue(backend::master, level, m, known_active);
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
// from: upcxx/os_env.hpp

namespace upcxx {

  template<>
  bool os_env(const std::string &name, const bool &otherwise) {
    return !!gasnett_getenv_yesno_withdefault(name.c_str(), otherwise);
  }
  int64_t os_env(const std::string &name, const int64_t &otherwise, size_t mem_size_multiplier) {
    return gasnett_getenv_int_withdefault(name.c_str(), otherwise, mem_size_multiplier);
  }
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

