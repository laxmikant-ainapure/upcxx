#include <upcxx/backend/gasnet/runtime.hpp>

#include <upcxx/backend/gasnet/rpc_inbox.hpp>
#include <upcxx/os_env.hpp>

#include <cstring>

#include <gasnet.h>
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

#if UPCXX_GASNET1_SEQ && !GASNET_SEQ
  #error "This backend is gasnet-seq only!"
#endif

#if UPCXX_GASNETEX_PAR && !GASNET_PAR
  #error "This backend is gasnet-par only!"
#endif

static_assert(
  sizeof(gasnet_handle_t) <= sizeof(uintptr_t),
  "gasnet_handle_t doesn't fit into a machine word!"
);

////////////////////////////////////////////////////////////////////////
// from: upcxx/backend.hpp

int backend::init_count = 0;
  
intrank_t backend::rank_n = -1;
intrank_t backend::rank_me; // leave undefined so valgrind can catch it.

persona backend::master;
persona_scope *backend::initial_master_scope = nullptr;

////////////////////////////////////////////////////////////////////////
// from: upcxx/backend/gasnet/runtime.hpp

size_t gasnet::am_size_rdzv_cutover;

////////////////////////////////////////////////////////////////////////

namespace {
  rpc_inbox rpcs_internal_;
  rpc_inbox rpcs_user_;
  
  #if UPCXX_GASNET1_SEQ
    handle_cb_queue hcbs_;
  #elif UPCXX_GASNETEX_PAR
    // in par-mode personas carry their own handle_cb_queue in persona_state
  #endif
}

////////////////////////////////////////////////////////////////////////

namespace {
  void after_gasnet();
  
  enum {
    id_am_eager_restricted = 128,
    id_am_eager_master,
    id_am_eager_persona
  };
    
  void am_eager_restricted(gasnet_token_t, void *buf, size_t buf_size, gasnet_handlerarg_t buf_align);
  void am_eager_master(gasnet_token_t, void *buf, size_t buf_size, gasnet_handlerarg_t buf_align_and_level);
  void am_eager_persona(gasnet_token_t, void *buf, size_t buf_size, gasnet_handlerarg_t buf_align_and_level,
                        gasnet_handlerarg_t persona_ptr_lo, gasnet_handlerarg_t persona_ptr_hi);
}

////////////////////////////////////////////////////////////////////////

#if !GASXX_SEGMENT_EVERYTHING
  #include <upcxx/dl_malloc.h>
  
  namespace {
    std::mutex segment_lock_;
    mspace segment_mspace_;
  }
#endif
  
////////////////////////////////////////////////////////////////////////
// from: upcxx/backend.hpp

void upcxx::init() {
  if(0 != backend::init_count++)
    return;
  
  int ok;
  
  ok = gasnet_init(nullptr, nullptr);
  UPCXX_ASSERT_ALWAYS(ok == GASNET_OK);
  
  gasnet_handlerentry_t am_table[] = {
    {id_am_eager_restricted, (void(*)())am_eager_restricted},
    {id_am_eager_master,     (void(*)())am_eager_master},
    {id_am_eager_persona,    (void(*)())am_eager_persona}
  };
  
  size_t segment_size = size_t(os_env<double>("UPCXX_SEGMENT_MB", 128)*(1<<20));
  // Do this instead? segment_size = gasnet_getMaxLocalSegmentSize();
  
  backend::rank_n = gasnet_nodes();
  backend::rank_me = gasnet_mynode();
  
  backend::initial_master_scope = new persona_scope{backend::master};
  
  ok = gasnet_attach(
    am_table, sizeof(am_table)/sizeof(am_table[0]),
    segment_size & -GASNET_PAGESIZE, // page size should always be a power of 2
    0
  );
  UPCXX_ASSERT_ALWAYS(ok == GASNET_OK);
  
  size_t am_medium_size = gasnet_AMMaxMedium();
  
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
  #if !GASXX_SEGMENT_EVERYTHING
    gasnet_seginfo_t *segs = new gasnet_seginfo_t[backend::rank_n];
    gasnet_getSegmentInfo(segs, backend::rank_n);
    gasnet_seginfo_t seg_me = segs[backend::rank_me];
    delete[] segs;
    
    segment_mspace_ = create_mspace_with_base(seg_me.addr, seg_me.size, 1);
    mspace_set_footprint_limit(segment_mspace_, seg_me.size);
  #endif
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
  #if !GASXX_SEGMENT_EVERYTHING
    #if UPCXX_GASNET1_SEQ
      UPCXX_ASSERT(backend::master.active_with_caller());
    #elif UPCXX_GASNETEX_PAR
      std::lock_guard<std::mutex> locked{segment_lock_};
    #endif
    
    void *p = mspace_memalign(segment_mspace_, alignment, size);
    //UPCXX_ASSERT(p != nullptr);
    return p;
  #else
    return operator new(size);
  #endif
}

void upcxx::deallocate(void *p) {
  #if !GASXX_SEGMENT_EVERYTHING
    #if UPCXX_GASNET1_SEQ
      UPCXX_ASSERT(backend::master.active_with_caller());
    #elif UPCXX_GASNETEX_PAR
      std::lock_guard<std::mutex> locked{segment_lock_};
    #endif
    
    mspace_free(segment_mspace_, p);
  #else
    operator delete(p);
  #endif
}

void backend::rma_get(
    void *buf_d,
    intrank_t rank_s,
    void const *buf_s,
    size_t buf_size,
    backend::rma_get_cb *cb
  ) {
  
  UPCXX_ASSERT(!UPCXX_GASNET1_SEQ || backend::master.active_with_caller());
  
  gasnet_handle_t handle = gasnet_get_nb_bulk(
    buf_d, rank_s, const_cast<void*>(buf_s), buf_size
  );
  cb->handle = reinterpret_cast<uintptr_t>(handle);
  
  #if UPCXX_GASNET1_SEQ
    hcbs_.enqueue(cb);
  #elif UPCXX_GASNETEX_PAR
    upcxx::current_persona().backend_state_.hcbs.enqueue(cb);
  #endif
  
  after_gasnet();
}

void backend::rma_put(
    intrank_t rank_d,
    void *buf_d,
    void const *buf_s,
    size_t buf_size,
    backend::rma_put_cb *cb
  ) {
  
  UPCXX_ASSERT(!UPCXX_GASNET1_SEQ || backend::master.active_with_caller());
  
  gasnet_handle_t handle = gasnet_put_nb_bulk(
    rank_d, buf_d, const_cast<void*>(buf_s), buf_size
  );
  cb->handle = reinterpret_cast<uintptr_t>(handle);
  
  #if UPCXX_GASNET1_SEQ
    hcbs_.enqueue(cb);
  #elif UPCXX_GASNETEX_PAR
    upcxx::current_persona().backend_state_.hcbs.enqueue(cb);
  #endif
  
  after_gasnet();
}

////////////////////////////////////////////////////////////////////////
// from: upcxx/backend/gasnet/runtime.hpp

void gasnet::send_am_eager_restricted(
    intrank_t recipient,
    void *buf,
    std::size_t buf_size,
    std::size_t buf_align
  ) {
  
  gasnet_AMRequestMedium1(recipient, id_am_eager_restricted, buf, buf_size, buf_align);
  
  after_gasnet();
}

void gasnet::send_am_eager_master(
    progress_level level,
    intrank_t recipient,
    void *buf,
    std::size_t buf_size,
    std::size_t buf_align
  ) {
  
  gasnet_AMRequestMedium1(
    recipient,
    id_am_eager_master,
    buf, buf_size,
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
  
  uintptr_t per_lo_u = reinterpret_cast<uintptr_t>(recipient_persona) & 0xffffffffu;
  uintptr_t per_hi_u = reinterpret_cast<uintptr_t>(recipient_persona) >> 31 >> 1;
  
  gasnet_handlerarg_t per_lo_h=0, per_hi_h=0;
  std::memcpy(&per_lo_h, &per_lo_u, sizeof(gasnet_handlerarg_t));
  std::memcpy(&per_hi_h, &per_hi_u, sizeof(gasnet_handlerarg_t));
  
  gasnet_AMRequestMedium3(
    recipient_rank,
    id_am_eager_persona,
    buf, buf_size,
    buf_align<<1 | (level == progress_level::user ? 1 : 0),
    per_lo_h, per_hi_h
  );
  
  after_gasnet();
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
      
      backend::rma_get(
        buf_d, rank_s, buf_s, buf_size,
        
        make_rma_get_cb<std::tuple<>>(
          std::tuple<>{},
          [=](std::tuple<>) {
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
        )
      );
    }
  );
}

template void gasnet::send_am_rdzv<progress_level::internal>(intrank_t, persona*, void*, size_t, size_t);
template void gasnet::send_am_rdzv<progress_level::user>(intrank_t, persona*, void*, size_t, size_t);

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
      #if UPCXX_GASNET1_SEQ
        exec_n += hcbs_.burst(4);
      #endif
      
      detail::persona_as_top(backend::master, [&]() {
        exec_n += rpcs_internal_.burst(100);
        exec_n += rpcs_user_.burst(100);
      });
    }
    
    detail::persona_foreach_active([&](persona &p) {
      #if UPCXX_GASNETEX_PAR
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
  void after_gasnet() {
    if(detail::tl_progressing >= 0)
      return;
    detail::tl_progressing = (int)progress_level::internal;
    
    bool have_master = UPCXX_GASNET1_SEQ || backend::master.active_with_caller();
    int total_exec_n = 0;
    int exec_n;
    
    do {
      exec_n = 0;
      
      if(have_master) {
        #if UPCXX_GASNET1_SEQ
          exec_n += hcbs_.burst(4);
        #endif
        
        detail::persona_as_top(backend::master, [&]() {
          exec_n += rpcs_internal_.burst(20);
        });
      }
      
      detail::persona_foreach_active([&](persona &p) {
        #if UPCXX_GASNETEX_PAR
          exec_n += p.backend_state_.hcbs.burst(4);
        #endif
        exec_n += detail::persona_burst(p, progress_level::internal);
      });
      
      total_exec_n += exec_n;
    }
    while(total_exec_n < 100 && exec_n != 0);
    
    detail::tl_progressing = -1;
  }
  
  void am_eager_restricted(
      gasnet_token_t,
      void *buf, size_t buf_size,
      gasnet_handlerarg_t buf_align
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
      gasnet_token_t,
      void *buf, size_t buf_size,
      gasnet_handlerarg_t buf_align_and_level
    ) {
    
    UPCXX_ASSERT(backend::rank_n!=-1);
    size_t buf_align = buf_align_and_level>>1;
    bool level_user = buf_align_and_level & 1;
    
    rpc_message *m = rpc_message::build_copy(buf, buf_size, buf_align);
    
    if(UPCXX_GASNETEX_PAR && !backend::master.active_with_caller()) {
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
      gasnet_token_t,
      void *buf, size_t buf_size,
      gasnet_handlerarg_t buf_align_and_level,
      gasnet_handlerarg_t per_lo_h,
      gasnet_handlerarg_t per_hi_h
    ) {
    
    UPCXX_ASSERT(backend::rank_n!=-1);
    size_t buf_align = buf_align_and_level>>1;
    bool level_user = buf_align_and_level & 1;
    
    uintptr_t per_lo_u=0, per_hi_u=0;
    std::memcpy(&per_lo_u, &per_lo_h, sizeof(gasnet_handlerarg_t));
    std::memcpy(&per_hi_u, &per_hi_h, sizeof(gasnet_handlerarg_t));
    
    persona *per = reinterpret_cast<persona*>(per_hi_u<<31<<1 | per_lo_u);
    per = per == nullptr ? &backend::master : per;
    
    rpc_message *m = rpc_message::build_copy(buf, buf_size, buf_align);
    
    if(UPCXX_GASNETEX_PAR && (per != &backend::master || !per->active_with_caller())) {
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
