#ifndef _223a1448_cf6d_42a9_864f_57409c60efe9
#define _223a1448_cf6d_42a9_864f_57409c60efe9

#include <upcxx/backend.hpp>
#include <upcxx/command.hpp>

#include <upcxx/backend/gasnet/handle_cb.hpp>

#include <cstdint>
#include <cstdlib>

////////////////////////////////////////////////////////////////////////

#define UPCXX_GASNET1_SEQ  UPCXX_BACKEND_gasnet1_seq
#define UPCXX_GASNETEX_PAR UPCXX_BACKEND_gasnetex_par

////////////////////////////////////////////////////////////////////////
// declarations for: upcxx/backend/gasnet/runtime.cpp

namespace upcxx {
namespace backend {
namespace gasnet {
  extern std::size_t am_size_rdzv_cutover;
  
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
  void send_am_eager_master(
    progress_level level,
    intrank_t recipient,
    void *command_buf,
    std::size_t buf_size,
    std::size_t buf_align
  );
  void send_am_eager_persona(
    progress_level level,
    intrank_t recipient_rank,
    persona *recipient_persona,
    void *command_buf,
    std::size_t buf_size,
    std::size_t buf_align
  );
  
  // Send AM (packed command) via rendezvous, receiver executes druing `level`.
  template<progress_level level>
  void send_am_rdzv(
    intrank_t recipient_rank,
    persona *recipient_persona, // nullptr == master
    void *command_buf,
    std::size_t buf_size, std::size_t buf_align
  );
}}}

//////////////////////////////////////////////////////////////////////
// implementation of: upcxx/backend.hpp

namespace upcxx {
namespace backend {
  //////////////////////////////////////////////////////////////////////
  // during_level
  
  template<typename Fn>
  void during_level(std::integral_constant<progress_level, progress_level::internal>, Fn &&fn) {
    UPCXX_ASSERT(!UPCXX_GASNET1_SEQ || backend::master.active_with_caller());
    fn();
  }
  template<typename Fn>
  void during_level(std::integral_constant<progress_level, progress_level::user>, Fn &&fn) {
    UPCXX_ASSERT(!UPCXX_GASNET1_SEQ || backend::master.active_with_caller());
    
    persona &p = UPCXX_GASNET1_SEQ ? backend::master : *detail::tl_top_persona;
    
    detail::persona_during(
      p, progress_level::user, std::forward<Fn>(fn),
      /*known_active=*/std::true_type{}
    );
  }

  template<progress_level level, typename Fn>
  void during_level(Fn &&fn) {
    during_level(std::integral_constant<progress_level,level>{}, std::forward<Fn>(fn));
  }
  
  //////////////////////////////////////////////////////////////////////
  // send_am
  
  template<upcxx::progress_level level, typename Fn>
  void send_am_master(intrank_t recipient, Fn &&fn) {
    UPCXX_ASSERT(!UPCXX_GASNET1_SEQ || backend::master.active_with_caller());
    
    parcel_layout ub;
    command_size_ubound(ub, fn);
    
    bool eager = ub.size() <= gasnet::am_size_rdzv_cutover;
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
      gasnet::send_am_eager_master(level, recipient, buf, w.size(), w.alignment());
      std::free(buf);
    }
    else
      gasnet::send_am_rdzv<level>(recipient, /*master*/nullptr, buf, w.size(), w.alignment());
  }
  
  template<upcxx::progress_level level, typename Fn>
  void send_am_persona(
      intrank_t recipient_rank,
      persona *recipient_persona,
      Fn &&fn
    ) {
    UPCXX_ASSERT(!UPCXX_GASNET1_SEQ || backend::master.active_with_caller());
    
    parcel_layout ub;
    command_size_ubound(ub, fn);
    
    bool eager = ub.size() <= gasnet::am_size_rdzv_cutover;
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
      gasnet::send_am_eager_persona(level, recipient_rank, recipient_persona, buf, w.size(), w.alignment());
      std::free(buf);
    }
    else
      gasnet::send_am_rdzv<level>(recipient_rank, recipient_persona, buf, w.size(), w.alignment());
  }
  
  //////////////////////////////////////////////////////////////////////
  // rma_[put/get]_cb:
  
  struct rma_put_cb: gasnet::handle_cb {};
  struct rma_get_cb: gasnet::handle_cb {};
  
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
  
  namespace gasnet {
    template<typename State, typename SrcCx, typename OpCx>
    struct rma_put_cb_impl final: rma_put_cb_wstate<State> {
      SrcCx src_cx;
      OpCx op_cx;
      
      rma_put_cb_impl(State &&state, SrcCx &&src_cx, OpCx &&op_cx):
        rma_put_cb_wstate<State>{std::move(state)},
        src_cx{std::move(src_cx)},
        op_cx{std::move(op_cx)} {
      }
      
      void execute_and_delete() override {
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
      
      void execute_and_delete() override {
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
    return new gasnet::rma_put_cb_impl<State,SrcCx,OpCx>{
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
    return new gasnet::rma_get_cb_impl<State,OpCx>{
      std::move(state),
      std::move(op_cx)
    };
  }
}}

//////////////////////////////////////////////////////////////////////
// gasnet_seq1 implementations

namespace upcxx {
namespace backend {
namespace gasnet {
  //////////////////////////////////////////////////////////////////////
  // send_am_restricted
  
  template<typename Fn>
  void send_am_restricted(intrank_t recipient, Fn &&fn) {
    UPCXX_ASSERT(!UPCXX_GASNET1_SEQ || backend::master.active_with_caller());
    
    parcel_layout ub;
    command_size_ubound(ub, fn);
    
    bool eager = ub.size() <= gasnet::am_size_rdzv_cutover;
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
      gasnet::send_am_eager_restricted(recipient, buf, w.size(), w.alignment());
      std::free(buf);
    }
    else {
      gasnet::send_am_rdzv<progress_level::internal>(
        recipient, /*master*/nullptr, buf, w.size(), w.alignment()
      );
    }
  }
}}}

#endif
