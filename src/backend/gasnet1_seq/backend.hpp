#ifndef _223a1448_cf6d_42a9_864f_57409c60efe9
#define _223a1448_cf6d_42a9_864f_57409c60efe9

#include <upcxx/backend.hpp>
#include <upcxx/command.hpp>

#include <cstdint>

////////////////////////////////////////////////////////////////////////
// gasnet_seq1 declarations

namespace upcxx {
namespace backend {
namespace gasnet1_seq {
  extern std::size_t am_size_rdzv_cutover;
  
  struct rma_callback: backend::rma_callback {
    rma_callback *next;
    std::uintptr_t handle;
  };
  
  // Send AM (packed command), receiver executes in handler.
  void send_am_eager_restricted(
    intrank_t recipient,
    void *command_buf,
    std::size_t buf_size
  );
  
  // Send fully bound callable, receiver executes in handler.
  template<typename Fn>
  void send_am_restricted(intrank_t recipient, Fn &&fn);
  
  // Send AM (packed command), receiver executes in `level` progress.
  void send_am_eager_queued(
    progress_level level,
    intrank_t recipient,
    void *command_buf,
    std::size_t buf_size,
    std::size_t buf_align
  );
  
  // Send AM (packed command) via rendezvous, receiver executes druing `level`.
  void send_am_rdzv(
    progress_level level,
    intrank_t recipient,
    void *command_buf,
    std::size_t buf_size, std::size_t buf_align
  );
}}}

//////////////////////////////////////////////////////////////////////
// Implementations of: upcxx/backend.hpp

namespace upcxx {
namespace backend {
  //////////////////////////////////////////////////////////////////////
  // send_am
  
  template<upcxx::progress_level level, typename Fn>
  void send_am(intrank_t recipient, Fn &&fn) {
    parcel_layout ub;
    command_size_ubound(ub, fn);
    
    bool eager = ub.size() <= gasnet1_seq::am_size_rdzv_cutover;
    
    void *buf = eager
      ? operator new(ub.size())
      : upcxx::allocate(ub.size());
    
    // TODO: Handle large alignment requirements.
    UPCXX_ASSERT(0 == (reinterpret_cast<uintptr_t>(buf) & (ub.alignment()-1)));
    
    parcel_writer w{buf};
    command_pack(w, fn, ub.size());
    
    if(eager)
      gasnet1_seq::send_am_eager_queued(level, recipient, buf, w.size(), w.alignment());
    else
      gasnet1_seq::send_am_rdzv(level, recipient, buf, w.size(), w.alignment());
  }
  
  //////////////////////////////////////////////////////////////////////
  // make_rma_cb
  
  namespace gasnet1_seq {
    template<typename Fn>
    struct rma_callback_impl final: gasnet1_seq::rma_callback {
      Fn fn_;
      rma_callback_impl(Fn fn): fn_{std::move(fn)} {}
      
      void fire_and_delete() override {
        // Do our job.
        this->fn_();
        
        // The benefit of merging fire & delete in one-shot callbacks is
        // destruction can be inlined as long as we mark the class `final`.
        // With seperate fire() and delete() we would incur a virtual
        // call for each.
        delete this;
      }
    };
  }

  template<typename Fn>
  inline rma_callback* make_rma_cb(Fn fn) {
    return new gasnet1_seq::rma_callback_impl<Fn>(std::move(fn));
  }
}}

//////////////////////////////////////////////////////////////////////
// gasnet_seq1 implementations

namespace upcxx {
namespace backend {
namespace gasnet1_seq {
  //////////////////////////////////////////////////////////////////////
  // send_am_restricted
  
  template<typename Fn>
  void send_am_restricted(intrank_t recipient, Fn &&fn) {
    parcel_layout ub;
    command_size_ubound(ub, fn);
    
    bool eager = ub.size() <= gasnet1_seq::am_size_rdzv_cutover;
    
    void *buf = eager
      ? operator new(ub.size())
      : upcxx::allocate(ub.size());
    
    // TODO: Handle large alignment requirements.
    UPCXX_ASSERT(0 == (reinterpret_cast<uintptr_t>(buf) & (ub.alignment()-1)));
    
    parcel_writer w{buf};
    command_pack(w, fn, ub.size());
    
    if(eager)
      gasnet1_seq::send_am_eager_restricted(recipient, buf, w.size());
    else
      gasnet1_seq::send_am_rdzv(
        progress_level_internal,
        recipient, buf, w.size(), w.alignment()
      );
  }
}}}
#endif
