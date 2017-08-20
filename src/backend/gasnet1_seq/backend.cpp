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

bool gasnet1_seq::in_user_progress_ = false;

gasnet1_seq::action *gasnet1_seq::user_actions_head_ = nullptr;
gasnet1_seq::action **gasnet1_seq::user_actions_tailp_ = &gasnet1_seq::user_actions_head_;

////////////////////////////////////////////////////////////////////////

namespace {
  enum {
    id_am_eager_restricted = 128,
    id_am_eager_queued
  };
    
  void am_eager_restricted(gasnet_token_t, void *buf, size_t buf_size, gasnet_handlerarg_t buf_align);
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
  
  message_queue msgs_internal_;
  message_queue msgs_user_;
  
  struct rma_queue {
    rma_cb *head = nullptr;
    rma_cb **tailp = &this->head;
    bool bursting = false;
    
    rma_queue() = default;
    rma_queue(rma_queue const&) = delete;
    
    void enqueue(rma_cb *rma);
    bool burst(int burst_n = 100);
  };
  
  rma_queue rmas_internal_;
  
  bool user_actions_burst(int burst_n = 100);
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

namespace {
  int init_count_ = 0;
}

void upcxx::init() {
  if (init_count_++ != 0) return;
  //printf("initializing\n");
  
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
  UPCXX_ASSERT_ALWAYS(init_count_ > 0);
  if (--init_count_ != 0) return;
  //printf("finalizing\n");
  upcxx::barrier();
}

void upcxx::barrier() {
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

void backend::rma_get(
    void *buf_d,
    intrank_t rank_s,
    void *buf_s,
    size_t buf_size,
    backend::rma_get_cb *cb
  ) {
  
  gasnet_handle_t handle = gasnet_get_nb_bulk(buf_d, rank_s, buf_s, buf_size);
  cb->handle = reinterpret_cast<uintptr_t>(handle);
  rmas_internal_.enqueue(cb);
  
  // Always check for new internal actions after a gasnet call.
  msgs_internal_.burst();
  rmas_internal_.burst(4); // scanning is expensive, focus on front
}

void backend::rma_put(
    intrank_t rank_d,
    void *buf_d,
    void *buf_s,
    size_t buf_size,
    backend::rma_put_cb *cb
  ) {
  
  gasnet_handle_t handle = gasnet_put_nb_bulk(rank_d, buf_d, buf_s, buf_size);
  cb->handle = reinterpret_cast<uintptr_t>(handle);
  rmas_internal_.enqueue(cb);
  
  // Always check for new internal actions after a gasnet call.
  msgs_internal_.burst();
  rmas_internal_.burst(4); // scanning is expensive, focus on front
}

////////////////////////////////////////////////////////////////////////
// from: upcxx/backend/gasnet1_seq/backend.hpp

void gasnet1_seq::send_am_eager_restricted(
    intrank_t recipient,
    void *buf,
    std::size_t buf_size,
    std::size_t buf_align
  ) {
  
  gasnet_AMRequestMedium1(recipient, id_am_eager_restricted, buf, buf_size, buf_align);
  
  // Always check for new internal actions after a gasnet call.
  msgs_internal_.burst();
  rmas_internal_.burst(4); // scanning is expensive, focus on front
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
    buf_align<<1 | (level == progress_level::user ? 1 : 0)
  );
  
  // Always check for new internal actions after a gasnet call.
  msgs_internal_.burst();
  rmas_internal_.burst(4); // scanning is expensive, focus on front
}

void gasnet1_seq::send_am_rdzv(
    progress_level level,
    intrank_t rank_d,
    void *buf_s,
    size_t buf_size,
    size_t buf_align
  ) {
  
  intrank_t rank_s = backend::rank_me;
  
  backend::send_am<progress_level::internal>(
    rank_d,
    [=]() {
      // TODO: Elide rma_get (copy) for node-local sends with pointer
      // translation and execution directly from source buffer.
      
      void *buf_d = upcxx::allocate(buf_size, buf_align);
      
      backend::rma_get(
        buf_d, rank_s, buf_s, buf_size,
        
        make_rma_get_cb<progress_level>(
          level,
          [=](progress_level level) {
            
            // Notify source rank it can free buffer.
            gasnet1_seq::send_am_restricted(rank_s,
              [=]() { upcxx::deallocate(buf_s); }
            );
            
            backend::during_level(level, [=]() {
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

////////////////////////////////////////////////////////////////////////
// anonymous namespace

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
    future<> buf_done = command_execute(r);
    
    message *m_next = m->next;
    
    buf_done >> [=]() {
      operator delete(m->payload);
    };
    
    did_something = true;
    m = m_next;
  }
  
  this->head = m;
  if(this->head == nullptr)
    this->tailp = &this->head;
  
  this->bursting = false;
  
  return did_something;
}

inline void rma_queue::enqueue(rma_cb *rma) {
  rma->next = nullptr;
  *this->tailp = rma;
  this->tailp = &rma->next;
}

bool rma_queue::burst(int burst_n) {
  if(this->bursting)
    return false;
  
  this->bursting = true;
  
  bool did_something = false;
  rma_cb **pp = &this->head;
  
  while(burst_n-- && *pp != nullptr) {
    rma_cb *p = *pp;
    gasnet_handle_t handle = reinterpret_cast<gasnet_handle_t>(p->handle);
    
    if(GASNET_OK == gasnet_try_syncnb(handle)) {
      // remove from queue
      *pp = p->next;
      if(*pp == nullptr)
        this->tailp = pp;
      
      // do it!
      p->fire_and_delete();
      did_something = true;
    }
    else
      pp = &p->next;
  }
  
  this->bursting = false;
  
  return did_something;
}

namespace {
bool user_actions_burst(int burst_n) {
  bool did_something = false;
  
  // steal the global action list into a temporary
  action *tmp_head = user_actions_head_;
  action **tmp_tailp = user_actions_tailp_;
  // reset global action list to empty
  user_actions_head_ = nullptr;
  user_actions_tailp_ = &user_actions_head_;
  
  while(true) {
    // is local list occupied?
    if(tmp_head != nullptr) {
      action *next = tmp_head->next_;
      
      // do the action, this could push new work onto global list
      tmp_head->fire_and_delete();
      
      tmp_head = next;
      did_something = true;
      if(0 == --burst_n) break;
    }
    else {
      // local list exhausted, steal from global list again if possible
      
      tmp_tailp = &tmp_head;
      
      if(user_actions_head_ != nullptr) {
        tmp_head = user_actions_head_;
        tmp_tailp = user_actions_tailp_;
        user_actions_head_ = nullptr;
        user_actions_tailp_ = &user_actions_head_;
      }
      else
        break;
    }
  }
  
  // prepend local list to global list
  *tmp_tailp = user_actions_head_;
  user_actions_head_ = tmp_head;
  
  return did_something;
}
}

////////////////////////////////////////////////////////////////////////
// from: upcxx/backend.hpp

void upcxx::progress(progress_level lev) {
  UPCXX_ASSERT(!in_user_progress_);
  in_user_progress_ = true;
  
  int rounds = 0;
  bool did_something;
  bool did_something_ever = false;
  
  do {
    gasnet_AMPoll();
    
    did_something = false;
    did_something |= rmas_internal_.burst();
    did_something |= msgs_internal_.burst();
    
    if(lev == progress_level::user) {
      did_something |= msgs_user_.burst();
      did_something |= user_actions_burst();
    }
    
    did_something_ever |= did_something;
  }
  // Try really hard to do stuff before leaving attentiveness.
  while(did_something && rounds++ < 4);
  
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
  
  if(did_something_ever)
    consecutive_nothings = 0;
  else if(++consecutive_nothings == 10) {
    sched_yield();
    consecutive_nothings = 0;
  }
  
  in_user_progress_ = false;
}

////////////////////////////////////////////////////////////////////////
// anonymous namespace

namespace {
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
    
    void *msg_buf;
    int ok = posix_memalign(&msg_buf, buf_align, msg_size);
    UPCXX_ASSERT_ALWAYS(ok == 0);
    
    message *m = new((char*)msg_buf + msg_offset) message;
    m->payload = msg_buf;
    
    // The (void**) casts *might* inform memcpy that it can assume word
    // alignment.
    std::memcpy((void**)msg_buf, (void**)buf, buf_size);
    
    message_queue &q = level_user ? msgs_user_ : msgs_internal_;
    q.enqueue(m);
  }
}
