#ifndef _186bad93_2bea_4643_b31f_81839975287e
#define _186bad93_2bea_4643_b31f_81839975287e

#include <upcxx/backend.hpp>
#include <upcxx/completion.hpp>
#include <upcxx/global_ptr.hpp>

namespace upcxx {
  namespace detail {
    struct rget_byref;
    template<typename T> struct rget_byval;
    
    template<typename Mode/*rget_byref|rget_byval<T>*/, typename Cx>
    struct rget_state_operxn;
    template<typename Cx>
    struct rget_state_remote;
    
    template<>
    struct rget_state_operxn<rget_byref, nil_cx> {
      rget_state_operxn(nil_cx) {}
      
      void completed() {}
    };
    template<typename T>
    struct rget_state_operxn<rget_byval<T>, nil_cx> {
      rget_state_operxn(nil_cx) {}
      
      void completed(T *buffer) {}
    };
    
    template<int ordinal>
    struct rget_state_operxn<rget_byref, future_cx<ordinal>> {
      promise<> pro;
      
      rget_state_operxn(future_cx<ordinal>) {}
      
      void completed() {
        backend::during_user(std::move(pro));
      }
    };
    
    template<typename T, int ordinal>
    struct rget_state_operxn<rget_byval<T>, future_cx<ordinal>> {
      promise<T> pro;
      
      rget_state_operxn(future_cx<ordinal>) {}
      
      void completed(T *buf_d) {
        struct deferred {
          promise<T> pro;
          T *buf_d;
          
          void operator()() {
            pro.fulfill_result(std::move(*buf_d));
            delete buf_d;
          }
        };
        
        backend::during_user(deferred{std::move(pro), buf_d});
      }
    };
    
    template<>
    struct rget_state_operxn<rget_byref, promise_cx<>> {
      promise<> *pro;
      
      rget_state_operxn(promise_cx<> &&cx):
        pro{&cx.pro_} {
      }
      
      void completed() {
        backend::during_user(pro);
      }
    };
    template<typename T>
    struct rget_state_operxn<rget_byval<T>, promise_cx<T>> {
      promise<T> *pro;
      
      rget_state_operxn(promise_cx<T> &&cx):
        pro{&cx.pro_} {
      }
      
      void completed(T *buf_d) {
        promise<T> *pro1 = this->pro; // avoid "this" capture
        backend::during_user([=]() {
          pro1->fulfill_result(std::move(*buf_d));
          delete buf_d;
        });
      }
    };
    
    template<>
    struct rget_state_remote<nil_cx> {
      rget_state_remote(nil_cx) {}
      
      void completed(intrank_t owner) {}
    };
    
    template<typename Fn>
    struct rget_state_remote<rpc_cx<Fn>> {
      Fn fn;
      
      rget_state_remote(rpc_cx<Fn> &&cx):
        fn{std::move(cx.fn_)} {
      }
      
      void completed(intrank_t owner) {
        backend::send_am_master<progress_level::user>(owner, std::move(fn));
      }
    };
    
    template<typename Mode, typename R, typename O>
    struct rget_states {
      intrank_t owner;
      rget_state_remote<R> r;
      rget_state_operxn<Mode,O> o;
      
      rget_states(intrank_t owner, completions<nil_cx,R,O> &&cxs):
        owner{owner},
        r{std::move(cxs.remote)},
        o{std::move(cxs.operxn)} {
      }
    };
    
    template<typename Mode, typename R, typename O>
    struct rget_futures_of {
      using return_type = void;
      
      rget_futures_of(rget_states<Mode,R,O> &st) {}
      
      void operator()() {}
    };
    
    template<typename R>
    struct rget_futures_of<rget_byref, R, future_cx<0>> {
      using return_type = future<>;
      
      future<> fut_;
      
      rget_futures_of(rget_states<rget_byref, R, future_cx<0>> &st):
        fut_{st.o.pro.get_future()} {
      }
      
      future<> operator()() { return std::move(fut_); }
    };
    
    template<typename T, typename R>
    struct rget_futures_of<rget_byval<T>, R, future_cx<0>> {
      using return_type = future<T>;
      
      future<T> fut_;
      
      rget_futures_of(rget_states<rget_byval<T>, R, future_cx<0>> &st):
        fut_{st.o.pro.get_future()} {
      }
      
      future<T> operator()() { return std::move(fut_); }
    };
  }
  
  //////////////////////////////////////////////////////////////////////
  // rget
  
  template<typename T,
           typename R = nil_cx,
           typename O = future_cx<0>>
  typename detail::rget_futures_of<detail::rget_byval<T>,R,O>::return_type
  rget(
      global_ptr<T> gp_s,
      completions<nil_cx,R,O> cxs = completions<nil_cx,R,O>{}
    ) {
    
    using state_type = detail::rget_states<detail::rget_byval<T>, R, O>;
    
    T *buf_d = new T;
    
    backend::rma_get_cb_wstate<state_type> *cb = backend::make_rma_get_cb<state_type>(
      state_type{
        gp_s.rank_, std::move(cxs)
      },
      /*operxn_cx*/[=](state_type &st) {
        st.r.completed(st.owner);
        st.o.completed(buf_d);
      }
    );
    
    auto answerer = detail::rget_futures_of<detail::rget_byval<T>,R,O>(cb->state);
    
    backend::rma_get(buf_d, gp_s.rank_, gp_s.raw_ptr_, sizeof(T), cb);
    
    return answerer();
  }
  
  template<typename T,
           typename R = nil_cx,
           typename O = future_cx<0>>
  typename detail::rget_futures_of<detail::rget_byref,R,O>::return_type
  rget(
      global_ptr<T> gp_s,
      T *buf_d, std::size_t n,
      completions<nil_cx,R,O> cxs = completions<nil_cx,R,O>{}
    ) {
    
    using state_type = detail::rget_states<detail::rget_byref, R, O>;
    
    backend::rma_get_cb_wstate<state_type> *cb = backend::make_rma_get_cb<state_type>(
      state_type{
        gp_s.rank_, std::move(cxs)
      },
      /*operxn_cx*/[](state_type &st) {
        st.r.completed(st.owner);
        st.o.completed();
      }
    );
    
    auto answerer = detail::rget_futures_of<detail::rget_byref,R,O>(cb->state);
    
    backend::rma_get(buf_d, gp_s.rank_, gp_s.raw_ptr_, n*sizeof(T), cb);
    
    return answerer();
  }
}
#endif
