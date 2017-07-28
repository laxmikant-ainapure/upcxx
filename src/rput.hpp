#ifndef _f6435716_8dd3_47f3_9519_bf1663d2cb80
#define _f6435716_8dd3_47f3_9519_bf1663d2cb80

#include <upcxx/backend.hpp>
#include <upcxx/completion.hpp>

namespace upcxx {
  namespace detail {
    struct rput_byref;
    template<typename T> struct rput_byval;
    
    template<typename Cx>
    struct rput_state_here;
    template<typename Cx>
    struct rput_state_remote;
    
    template<>
    struct rput_state_here<nil_cx> {
      rput_state_here(nil_cx) {}
      
      void completed() {}
    };
    
    template<int ordinal>
    struct rput_state_here<future_cx<ordinal>> {
      promise<> pro;
      
      rput_state_here(future_cx<ordinal>) {}
      
      void completed() {
        backend::during_user(std::move(pro));
      }
    };
    
    template<>
    struct rput_state_here<promise_cx<>> {
      promise<> *pro;
      
      rput_state_here(promise_cx<> &cx):
        pro{&cx.pro_} {
      }
      
      void completed() {
        backend::during_user(pro);
      }
    };
    
    template<>
    struct rput_state_remote<nil_cx> {
      rput_state_remote(nil_cx) {}
      
      void completed(intrank_t target) {}
    };
    
    template<typename Fn>
    struct rput_state_remote<rpc_cx<Fn>> {
      Fn fn;
      
      rput_state_remote(rpc_cx<Fn> &&cx):
        fn{std::move(cx.fn_)} {
      }
      
      void completed(intrank_t target) {
        backend::send_am<progress_level_user>(target, std::move(fn));
      }
    };
    
    template<typename Mode, typename S, typename R, typename O>
    struct rput_states;
    
    template<typename S, typename R, typename O>
    struct rput_states<rput_byref, S, R, O> {
      intrank_t target;
      rput_state_here<S> s;
      rput_state_remote<R> r;
      rput_state_here<O> o;
      
      rput_states(intrank_t target, completions<S,R,O> &&cxs):
        target{target},
        s{std::move(cxs.source)},
        r{std::move(cxs.remote)},
        o{std::move(cxs.operxn)} {
      }
    };
    
    template<typename T, typename S, typename R, typename O>
    struct rput_states<rput_byval<T>, S, R, O>: rput_states<rput_byref, S, R, O> {
      T buffer;
      
      rput_states(T &&value, intrank_t target, completions<S,R,O> &&cxs):
        rput_states<rput_byref, S, R, O>{target, std::move(cxs)},
        buffer{std::move(value)} {
      }
    };
    
    template<typename S, typename R, typename O>
    struct rput_futures_of {
      using return_type = void;
      
      template<typename Mode>
      rput_futures_of(rput_states<Mode,S,R,O> &st) {}
      
      void operator()() {}
    };
    
    template<typename R, typename O, int S_ord>
    struct rput_futures_of<future_cx<S_ord>,R,O> {
      using return_type = future<>;
      
      future<> fut_;
      
      template<typename Mode>
      rput_futures_of(rput_states<Mode,future_cx<S_ord>,R,O> &st):
        fut_{st.s.pro.get_future()} {
      }
      
      future<> operator()() { return std::move(fut_); }
    };
    
    template<typename S, typename R, int O_ord>
    struct rput_futures_of<S,R,future_cx<O_ord>> {
      using return_type = future<>;
      
      future<> fut_;
      
      template<typename Mode>
      rput_futures_of(rput_states<Mode,S,R,future_cx<O_ord>> &st):
        fut_{st.o.pro.get_future()} {
      }
      
      future<> operator()() { return std::move(fut_); }
    };
    
    template<int S_ord, typename R, int O_ord>
    struct rput_futures_of<future_cx<S_ord>, R, future_cx<O_ord>> {
      using return_type = std::tuple<future<>,future<>>;
      
      future<> s_, o_;
      
      template<typename Mode>
      rput_futures_of(rput_states<Mode,future_cx<S_ord>, R, future_cx<O_ord>> &st):
        s_{st.s.pro.get_future()},
        o_{st.o.pro.get_future()} {
      }
      
      std::tuple<future<>, future<>> operator()() {
        return std::tuple<future<>, future<>>{
          std::move(S_ord == 0 ? s_ : o_),
          std::move(O_ord == 1 ? o_ : s_)
        };
      }
    };
  }
  
  //////////////////////////////////////////////////////////////////////
  // rput: WRONG API, these should be global_ptr, not virtual address (T*)
  
  template<typename T,
           typename S = nil_cx,
           typename R = nil_cx,
           typename O = future_cx<0>>
  typename detail::rput_futures_of<S,R,O>::return_type
  rput(
      T value_s,
      intrank_t rank_d, T *ptr_d,
      completions<S,R,O> cxs = completions<S,R,O>{}
    ) {
    
    using state_type = detail::rput_states<detail::rput_byval<T>, S, R, O>;
    
    backend::rma_put_cb_wstate<state_type> *cb = backend::make_rma_put_cb<state_type>(
      state_type{
        std::move(value_s), rank_d, std::move(cxs)
      },
      /*source_cx*/[](state_type &st) {
        st.s.completed();
      },
      /*operxn_cx*/[](state_type &st) {
        st.r.completed(st.target);
        st.o.completed();
      }
    );
    
    auto answerer = detail::rput_futures_of<S,R,O>(cb->state);
    
    backend::rma_put(rank_d, ptr_d, &cb->state.buffer, sizeof(T), cb);
    
    return answerer();
  }
  
  template<typename T,
           typename S = nil_cx,
           typename R = nil_cx,
           typename O = future_cx<0>>
  typename detail::rput_futures_of<S,R,O>::return_type
  rput(
      T const *buf_s, std::size_t n,
      intrank_t rank_d, T *ptr_d,
      completions<S,R,O> cxs = completions<S,R,O>{}
    ) {
    
    using state_type = detail::rput_states<detail::rput_byref, S, R, O>;
    
    backend::rma_put_cb_wstate<state_type> *cb = backend::make_rma_put_cb<state_type>(
      state_type{
        rank_d, std::move(cxs)
      },
      /*source_cx*/[](state_type &st) {
        st.s.completed();
      },
      /*operxn_cx*/[](state_type &st) {
        st.r.completed(st.target);
        st.o.completed();
      }
    );
    
    auto answerer = detail::rput_futures_of<S,R,O>(cb->state);
    
    backend::rma_put(rank_d, ptr_d, buf_s, n*sizeof(T), cb);
    
    return answerer();
  }
}
#endif
