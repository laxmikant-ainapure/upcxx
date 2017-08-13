#ifndef _223a1448_cf6d_42a9_864f_57409c60efe9
#define _223a1448_cf6d_42a9_864f_57409c60efe9

#include <upcxx/backend.hpp>
#include <upcxx/command.hpp>

#include <cstdint>
#include <cstdlib>

////////////////////////////////////////////////////////////////////////
// declarations for: upcxx/backend/gasnet1_seq/backend.cpp

namespace upcxx {
namespace backend {
  namespace gasnet1_seq {
    extern std::size_t am_size_rdzv_cutover;
    
    struct action {
      action *next_;
      virtual void fire_and_delete() = 0;
    };
    
    extern bool in_user_progress_;
    extern action *user_actions_head_;
    extern action **user_actions_tailp_;
    
    // Send AM (packed command), receiver executes in handler.
    void send_am_eager_restricted(
      intrank_t recipient,
      void *command_buf,
      std::size_t buf_size,
      std::size_t buf_align
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
    
    struct rma_cb {
      rma_cb *next;
      std::uintptr_t handle;
      
      virtual void fire_and_delete() = 0;
    };
  }
}}

//////////////////////////////////////////////////////////////////////
// implementation of: upcxx/backend.hpp

namespace upcxx {
namespace backend {
  //////////////////////////////////////////////////////////////////////
  // during_user_progress
  
  namespace gasnet1_seq {
    template<typename Fn>
    struct action_impl final: action {
      Fn fn;
      action_impl(Fn fn): fn{std::move(fn)} {}
      
      void fire_and_delete() {
        fn();
        delete this;
      }
    };
  }
  
  template<typename Fn1>
  inline void during_user(Fn1 &&fn) {
    using Fn = typename std::decay<Fn1>::type;
    
    if(gasnet1_seq::in_user_progress_)
      fn();
    else {
      auto *a = new gasnet1_seq::action_impl<Fn>{std::forward<Fn1>(fn)};
      // enqueue a on `user_actions`
      a->next_ = nullptr;
      *(gasnet1_seq::user_actions_tailp_) = a;
      gasnet1_seq::user_actions_tailp_ = &a->next_;
    }
  }
  
  inline void during_user(promise<> &&pro) {
    struct deferred {
      promise<> pro;
      void operator()() { pro.fulfill_result(); }
    };
    during_user(deferred{std::move(pro)});
  }
  
  inline void during_user(promise<> *pro) {
    during_user([=]() { pro->fulfill_result(); });
  }
  
  template<typename Fn1>
  inline void during_level(progress_level level, Fn1 &&fn) {
    using Fn = typename std::decay<Fn1>::type;
    
    if(level == progress_level::internal || gasnet1_seq::in_user_progress_)
      fn();
    else {
      auto *a = new gasnet1_seq::action_impl<Fn>{std::forward<Fn1>(fn)};
      // enqueue a on `user_actions`
      a->next_ = nullptr;
      *(gasnet1_seq::user_actions_tailp_) = a;
      gasnet1_seq::user_actions_tailp_ = &a->next_;
    }
  }
  
  //////////////////////////////////////////////////////////////////////
  // send_am
  
  template<upcxx::progress_level level, typename Fn>
  void send_am(intrank_t recipient, Fn &&fn) {
    parcel_layout ub;
    command_size_ubound(ub, fn);
    
    bool eager = ub.size() <= gasnet1_seq::am_size_rdzv_cutover;
    void *buf;
    
    if(eager) {
      int ok = posix_memalign(&buf, ub.alignment(), ub.size());
      UPCXX_ASSERT_ALWAYS(ok == 0);
    }
    else
      buf = upcxx::allocate(ub.size(), ub.alignment());
    
    parcel_writer w{buf};
    command_pack(w, ub.size(), fn);
    
    if(eager) {
      gasnet1_seq::send_am_eager_queued(level, recipient, buf, w.size(), w.alignment());
      std::free(buf);
    }
    else
      gasnet1_seq::send_am_rdzv(level, recipient, buf, w.size(), w.alignment());
  }
  
  //////////////////////////////////////////////////////////////////////
  // rma_[put/get]_cb:
  
  struct rma_put_cb: gasnet1_seq::rma_cb {};
  struct rma_get_cb: gasnet1_seq::rma_cb {};
  
  template<typename State>
  struct rma_put_cb_wstate: rma_put_cb {
    State state;
    rma_put_cb_wstate(State &&st): state{std::move(st)} {}
  };
  template<typename State>
  struct rma_get_cb_wstate: rma_get_cb {
    State state;
    rma_get_cb_wstate(State &&st): state{std::move(st)} {}
  };
  
  //////////////////////////////////////////////////////////////////////
  // make_rma_[put/get]_cb
  
  namespace gasnet1_seq {
    template<typename State, typename SrcCx, typename OpCx>
    struct rma_put_cb_impl final: rma_put_cb_wstate<State> {
      SrcCx src_cx;
      OpCx op_cx;
      
      rma_put_cb_impl(State &&state, SrcCx &&src_cx, OpCx &&op_cx):
        rma_put_cb_wstate<State>{std::move(state)},
        src_cx{std::move(src_cx)},
        op_cx{std::move(op_cx)} {
      }
      
      void fire_and_delete() override {
        src_cx(this->state);
        op_cx(this->state);
        delete this;
      }
    };
    
    template<typename State, typename OpCx>
    struct rma_get_cb_impl final: rma_get_cb_wstate<State> {
      OpCx op_cx;
      
      rma_get_cb_impl(State &&state, OpCx &&op_cx):
        rma_get_cb_wstate<State>{std::move(state)},
        op_cx{std::move(op_cx)} {
      }
      
      void fire_and_delete() override {
        op_cx(this->state);
        delete this;
      }
    };
  }
  
  template<typename State, typename SrcCx, typename OpCx>
  inline rma_put_cb_wstate<State>* make_rma_put_cb(
      State state,
      SrcCx src_cx,
      OpCx op_cx
    ) {
    return new gasnet1_seq::rma_put_cb_impl<State,SrcCx,OpCx>{
      std::move(state),
      std::move(src_cx),
      std::move(op_cx)
    };
  }
  
  template<typename State, typename OpCx>
  inline rma_get_cb_wstate<State>* make_rma_get_cb(
      State state,
      OpCx op_cx
    ) {
    return new gasnet1_seq::rma_get_cb_impl<State,OpCx>{
      std::move(state),
      std::move(op_cx)
    };
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
    void *buf;
    
    if(eager) {
      int ok = posix_memalign(&buf, ub.alignment(), ub.size());
      UPCXX_ASSERT_ALWAYS(ok == 0);
    }
    else
      buf = upcxx::allocate(ub.size(), ub.alignment());
    
    parcel_writer w{buf};
    command_pack(w, ub.size(), fn);
    
    if(eager) {
      gasnet1_seq::send_am_eager_restricted(recipient, buf, w.size(), w.alignment());
      std::free(buf);
    }
    else {
      gasnet1_seq::send_am_rdzv(
        progress_level::internal,
        recipient, buf, w.size(), w.alignment()
      );
    }
  }
}}}
#endif
