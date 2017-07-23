#include <upcxx/os_env.hpp>
#include <upcxx/backend/gasnet1_seq/backend.hpp>

#include <cstring>

#include <gasnet.h>
#include <unistd.h>

using namespace upcxx;
using namespace upcxx::backend;
using namespace upcxx::backend::gasnet1_seq;

using namespace std;

////////////////////////////////////////////////////////////////////////

#if !GASNET_SEQ
  #error "This backend is gasnet-seq only!"
#endif

static_assert(
  sizeof(gasnet_handle_t) <= sizeof(uintptr_t),
  "gasnet_handle_t doesn't fit into a machine word!"
);

////////////////////////////////////////////////////////////////////////
// from: upcxx/backend.hpp

intrank_t backend::rank_n;
intrank_t backend::rank_me;

////////////////////////////////////////////////////////////////////////
// from: upcxx/backend/gasnet1_seq/backend.hpp

size_t gasnet1_seq::am_size_rdzv_cutover;

////////////////////////////////////////////////////////////////////////

namespace {
  enum {
    id_am_eager_restricted = 128,
    id_am_eager_queued
  };
    
  void am_eager_restricted(gasnet_token_t, void *buf, size_t buf_size);
  void am_eager_queued(gasnet_token_t, void *buf, size_t buf_size, gasnet_handlerarg_t buf_align_and_level);
  
  struct message {
    message *next;
    void *payload;
  };
  
  struct message_queue {
    message *head = nullptr;
    message **tailp = &this->head;
    bool bursting = false;
    
    message_queue() = default;
    message_queue(message_queue const&) = delete;
    
    void enqueue(message *m);
    bool burst(int burst_n = 100);
  };
  
  message_queue msgs_internal;
  message_queue msgs_user;
  
  struct rma_queue {
    rma_callback *head = nullptr;
    rma_callback **tailp = &this->head;
    bool bursting = false;
    
    rma_queue() = default;
    rma_queue(rma_queue const&) = delete;
    
    void enqueue(rma_callback *rma);
    bool burst(int burst_n = 100);
  };
  
  rma_queue rmas_internal;
  rma_queue rmas_user;
}

////////////////////////////////////////////////////////////////////////

#if !GASXX_SEGMENT_EVERYTHING
  #include <upcxx/dl_malloc.h>
  
  namespace {
    mspace segment_mspace;
  }
#endif
  
////////////////////////////////////////////////////////////////////////
// from: upcxx/backend.hpp

#warning "TODO: Use counting semantics for init/finalize."

void upcxx::init() {
  int ok;
  
  ok = gasnet_init(nullptr, nullptr);
  UPCXX_ASSERT_ALWAYS(ok == GASNET_OK);
  
  gasnet_handlerentry_t am_table[] = {
    {id_am_eager_restricted, (void(*)())am_eager_restricted},
    {id_am_eager_queued,     (void(*)())am_eager_queued}
  };
  
  size_t segment_size = os_env<size_t>("UPCXX_SEGMENT_MB", 128<<20);
  // Do this instead? segment_size = gasnet_getMaxLocalSegmentSize();
  
  ok = gasnet_attach(
    am_table, sizeof(am_table)/sizeof(am_table[0]),
    segment_size & -GASNET_PAGESIZE, // page size should always be a power of 2
    0
  );
  UPCXX_ASSERT_ALWAYS(ok == GASNET_OK);
  
  backend::rank_n = gasnet_nodes();
  backend::rank_me = gasnet_mynode();
  
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
  gasnet1_seq::am_size_rdzv_cutover =
    am_medium_size < 1<<10 ? 256 :
    am_medium_size < 8<<10 ? 512 :
                             1024;
  
  // setup shared segment allocator
  #if !GASXX_SEGMENT_EVERYTHING
    gasnet_seginfo_t *segs = new gasnet_seginfo_t[backend::rank_n];
    gasnet_getSegmentInfo(segs, backend::rank_n);
    gasnet_seginfo_t seg_me = segs[backend::rank_me];
    delete[] segs;
    
    segment_mspace = create_mspace_with_base(seg_me.addr, seg_me.size, 1);
    mspace_set_footprint_limit(segment_mspace, seg_me.size);
  #endif
}

void upcxx::finalize() {
  gasnet_barrier_notify(0, GASNET_BARRIERFLAG_ANONYMOUS);
  
  while(GASNET_OK != gasnet_barrier_try(0, GASNET_BARRIERFLAG_ANONYMOUS))
    upcxx::progress();
}

void* upcxx::allocate(size_t size, size_t alignment) {
  #if !GASXX_SEGMENT_EVERYTHING
    void *p = mspace_memalign(segment_mspace, alignment, size);
    UPCXX_ASSERT(p != nullptr);
    return p;
  #else
    return operator new(size);
  #endif
}

void upcxx::deallocate(void *p) {
  #if !GASXX_SEGMENT_EVERYTHING
    mspace_free(segment_mspace, p);
  #else
    operator delete(p);
  #endif
}

////////////////////////////////////////////////////////////////////////
// from: upcxx/backend/gasnet1_seq/backend.hpp

void gasnet1_seq::send_am_eager_restricted(
    intrank_t recipient,
    void *buf,
    std::size_t buf_size
  ) {
  
  gasnet_AMRequestMedium0(recipient, id_am_eager_restricted, buf, buf_size);
  operator delete(buf);
  
  // Always check for new internal actions after a gasnet call.
  msgs_internal.burst();
  rmas_internal.burst(4); // scanning is expensive, focus on front
}

void gasnet1_seq::send_am_eager_queued(
    progress_level level,
    intrank_t recipient,
    void *buf,
    std::size_t buf_size,
    std::size_t buf_align
  ) {
  
  gasnet_AMRequestMedium1(
    recipient,
    id_am_eager_queued,
    buf, buf_size,
    buf_align<<1 | (level == progress_level_user ? 1 : 0)
  );
  operator delete(buf);
  
  // Always check for new internal actions after a gasnet call.
  msgs_internal.burst();
  rmas_internal.burst(4); // scanning is expensive, focus on front
}

void gasnet1_seq::rma_get(
    void *buf_d,
    intrank_t rank_s,
    void *buf_s,
    size_t buf_size,
    progress_level done_level,
    rma_callback *done
  ) {
  
  gasnet_handle_t handle = gasnet_get_nb_bulk(buf_d, rank_s, buf_s, buf_size);
  done->handle = reinterpret_cast<uintptr_t>(handle);
  
  rma_queue &q = done_level == progress_level_internal ? rmas_internal : rmas_user;
  q.enqueue(done);
  
  // Always check for new internal actions after a gasnet call.
  msgs_internal.burst();
  rmas_internal.burst(4); // scanning is expensive, focus on front
}

void gasnet1_seq::send_am_rdzv(
    progress_level level,
    intrank_t rank_d,
    void *buf_s,
    size_t buf_size,
    size_t buf_align
  ) {
  
  intrank_t rank_s = backend::rank_me;
  
  backend::send_am<progress_level_internal>(
    rank_d,
    [=]() {
      // TODO: Elide rma_get (copy) for node-local sends with pointer
      // translation and execution directly from source buffer.
      
      void *buf_d = upcxx::allocate(buf_size);
      
      // TODO: Handle large alignments.
      UPCXX_ASSERT(0 == (reinterpret_cast<uintptr_t>(buf_d) & (buf_align-1)));
      
      gasnet1_seq::rma_get(
        buf_d, rank_s, buf_s, buf_size,
        
        /*done_level*/level,
        
        /*done*/make_rma_cb([=]() {
          // Notify source rank it can free buffer.
          gasnet1_seq::send_am_restricted(rank_s,
            [=]() { upcxx::deallocate(buf_s); }
          );
          
          // Execute buffer.
          parcel_reader r{buf_d};
          command_unpack_and_execute(r);
          
          upcxx::deallocate(buf_d);
        })
      );
    }
  );
}

////////////////////////////////////////////////////////////////////////

inline void message_queue::enqueue(message *m) {
  m->next = nullptr;
  *this->tailp = m;
  this->tailp = &m->next;
}

bool message_queue::burst(int burst_n) {
  if(this->bursting)
    return false;
  
  this->bursting = true;
  
  bool did_something = false;
  message *m = this->head;
  
  while(burst_n-- && m != nullptr) {
    parcel_reader r{m->payload};
    command_unpack_and_execute(r);
    
    message *m_next = m->next;
    operator delete(m->payload); // contains message struct too
    
    did_something = true;
    m = m_next;
  }
  
  this->head = m;
  if(this->head == nullptr)
    this->tailp = &this->head;
  
  this->bursting = false;
  
  return did_something;
}

inline void rma_queue::enqueue(rma_callback *rma) {
  rma->next = nullptr;
  *this->tailp = rma;
  this->tailp = &rma->next;
}

bool rma_queue::burst(int burst_n) {
  if(this->bursting)
    return false;
  
  this->bursting = true;
  
  bool did_something = false;
  rma_callback **pp = &this->head;
  
  while(burst_n-- && *pp != nullptr) {
    rma_callback *p = *pp;
    gasnet_handle_t handle = reinterpret_cast<gasnet_handle_t>(p->handle);
    
    if(GASNET_OK == gasnet_try_syncnb(handle)) {
      // remove from queue
      *pp = p->next;
      if(*pp == nullptr)
        this->tailp = pp;
      
      p->fire_and_delete();
      did_something = true;
    }
    else
      pp = &p->next;
  }
  
  this->bursting = false;
  
  return did_something;
}

////////////////////////////////////////////////////////////////////////
// from: upcxx/backend.hpp

void upcxx::progress(progress_level lev) {
  int rounds = 0;
  bool did_something;
  bool did_something_ever = false;
  
  do {
    gasnet_AMPoll();
    
    did_something = false;
    did_something |= rmas_internal.burst();
    did_something |= msgs_internal.burst();
    
    if(lev == progress_level_user) {
      did_something |= rmas_user.burst();
      did_something |= msgs_user.burst();
    }
    
    did_something_ever |= did_something;
  }
  // Try really hard to do stuff before leaving attentiveness.
  while(did_something && rounds++ < 4);
  
  if(!did_something_ever) {
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
    static int consecutive_nothings = 0;
    
    if(++consecutive_nothings == 10) {
      sched_yield();
      consecutive_nothings = 0;
    }
  }
}

////////////////////////////////////////////////////////////////////////
// anonymous namespace

namespace {
  void am_eager_restricted(gasnet_token_t, void *buf, size_t) {
    parcel_reader r{buf};
    command_unpack_and_execute(r);
  }
  
  void am_eager_queued(
      gasnet_token_t,
      void *buf, size_t buf_size,
      gasnet_handlerarg_t buf_align_and_level
    ) {
    
    size_t buf_align = buf_align_and_level>>1;
    bool level_user = buf_align_and_level & 1;
    
    size_t msg_size = buf_size;
    msg_size = (msg_size + alignof(message)-1) & -alignof(message);
    
    size_t msg_offset = msg_size;
    msg_size += sizeof(message);
    
    char *msg_buf = (char*)operator new(msg_size);
    
    // TODO: use posix_memalign instead of operator new to handle this
    // case (could be caused by simd serialization code).
    UPCXX_ASSERT((reinterpret_cast<uintptr_t>(msg_buf) & (buf_align-1)) == 0);
    
    message *m = new(msg_buf + msg_offset) message;
    m->payload = msg_buf;
    
    // The (void**) casts *might* inform memcpy that it can assume word
    // alignment.
    std::memcpy((void**)msg_buf, (void**)buf, buf_size);
    
    message_queue &q = level_user ? msgs_user : msgs_internal;
    q.enqueue(m);
  }
}
