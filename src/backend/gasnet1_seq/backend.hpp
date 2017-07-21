#ifndef _223a1448_cf6d_42a9_864f_57409c60efe9
#define _223a1448_cf6d_42a9_864f_57409c60efe9

#include <upcxx/backend.hpp>
#include <upcxx/command.hpp>

#include <cstdint>

////////////////////////////////////////////////////////////////////////

namespace upcxx {
namespace backend {
namespace gasnet1_seq {
  extern std::size_t am_size_rdzv_cutover;
  
  struct rma_callback {
    rma_callback *next;
    std::uintptr_t handle;
    
    virtual void fire_and_delete() = 0;
  };
  
  template<typename Fn>
  rma_callback* make_rma_cb(Fn fn);
  
  // Send AM (packed command), receiver executes in handler.
  void send_am_restricted(
    intrank_t recipient,
    void *command_buf,
    std::size_t buf_size
  );
  
  // Send fully bound callable, receiver executes in handler.
  template<typename Fn>
  void send_am_restricted(intrank_t recipient, Fn &&fn);
  
  // Send AM (packed command), receiver executes in `level` progress.
  void send_am_queued(
    progress_level level,
    intrank_t recipient,
    void *command_buf,
    std::size_t buf_size,
    std::size_t buf_align
  );
  
  // Do a GET, execute `done` callback in `done_level` progress when finished.
  // (xxx_d = dest, xxx_s = source)
  void rma_get(
    void *buf_d,
    intrank_t rank_s,
    void *buf_s,
    std::size_t buf_size,
    progress_level done_level,
    rma_callback *done
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
// from: upcxx/backend.hpp

template<upcxx::progress_level level, typename Fn>
void upcxx::backend::send_am(intrank_t recipient, Fn &&fn) {
  parcel_layout ub;
  command_size_ubound(ub, fn);
  
  bool small = ub.size() <= gasnet1_seq::am_size_rdzv_cutover;
  
  void *buf = small
    ? operator new(ub.size())
    : upcxx::allocate(ub.size());
  
  // TODO: Handle large alignment requirements.
  UPCXX_ASSERT(0 == (reinterpret_cast<uintptr_t>(buf) & (ub.alignment()-1)));
  
  parcel_writer w{buf};
  command_pack(w, fn, ub.size());
  
  if(small)
    gasnet1_seq::send_am_queued(level, recipient, buf, w.size(), w.alignment());
  else
    gasnet1_seq::send_am_rdzv(level, recipient, buf, w.size(), w.alignment());
}

////////////////////////////////////////////////////////////////////////

namespace upcxx {
namespace backend {
namespace gasnet1_seq {
  //////////////////////////////////////////////////////////////////////
  // make_callback
  
  template<typename Fn>
  struct rma_callback_impl final: rma_callback {
    Fn fn_;
    rma_callback_impl(Fn fn): fn_{std::move(fn)} {}
    
    void fire_and_delete() {
      this->fn_();
      delete this;
    }
  };
  
  template<typename Fn>
  inline rma_callback* make_rma_cb(Fn fn) {
    return new rma_callback_impl<Fn>(std::move(fn));
  }
  
  //////////////////////////////////////////////////////////////////////
  // send_am_restricted
  
  template<typename Fn>
  void send_am_restricted(intrank_t recipient, Fn &&fn) {
    parcel_layout ub;
    command_size_ubound(ub, fn);
    
    bool small = ub.size() <= gasnet1_seq::am_size_rdzv_cutover;
    
    void *buf = small
      ? operator new(ub.size())
      : upcxx::allocate(ub.size());
    
    // TODO: Handle large alignment requirements.
    UPCXX_ASSERT(0 == (reinterpret_cast<uintptr_t>(buf) & (ub.alignment()-1)));
    
    parcel_writer w{buf};
    command_pack(w, fn, ub.size());
    
    if(small)
      gasnet1_seq::send_am_restricted(recipient, buf, w.size());
    else
      gasnet1_seq::send_am_rdzv(progress_level_internal, recipient, buf, w.size(), w.alignment());
  }
}}}
#endif
