#ifndef _f6435716_8dd3_47f3_9519_bf1663d2cb80
#define _f6435716_8dd3_47f3_9519_bf1663d2cb80

#include <upcxx/backend.hpp>
#include <upcxx/completion.hpp>
#include <upcxx/global_ptr.hpp>

// For the time being, our implementation of put/get requires the
// gasnet backend. Ideally we would detect gasnet via UPCXX_BACKEND_GASNET
// and if not present, rely on a reference implementation over
// upcxx::backend generic API.
#include <upcxx/backend/gasnet/runtime.hpp>

namespace upcxx {
  namespace detail {
    enum class rma_put_source_mode { now, defer, handle };
    
    // rma_put_nb: Does the actual gasnet non-blocking put
    template<rma_put_source_mode src_mode>
    void rma_put_nb(
      intrank_t rank_d, void *buf_d,
      const void *buf_s, std::size_t size,
      backend::gasnet::handle_cb *source_cb,
      backend::gasnet::handle_cb *operation_cb
    );

    // rma_put_b: Does the actual gasnet blocking put
    void rma_put_b(
      intrank_t rank_d, void *buf_d,
      const void *buf_s, std::size_t size
    );

    ////////////////////////////////////////////////////////////////////
    // rput_event_values: Value for completions_state's EventValues
    // template argument. rput events always report no values.
    struct rput_event_values {
      template<typename Event>
      using tuple_t = std::tuple<>;
    };
    
    ////////////////////////////////////////////////////////////////////

    // In the following classes, FinalType will be one of:
    //   rput_cbs_byref<CxStateHere, CxStateRemote>
    //   rput_cbs_byval<T, CxStateHere, CxStateRemote>
    // So there is a pattern of a class inheriting a base-class which has
    // the derived class as a template argument. This allows the base
    // class to access the derived class without using virtual functions.
    
    // rput_cb_source: rput_cbs_{byref|byval} inherits this to hold
    // source-copmletion details.
    template<typename FinalType, typename CxStateHere, typename CxStateRemote,
             bool sync = completions_is_event_sync<
                 typename CxStateHere::completions_t,
                 source_cx_event
               >::value,
             // only use handle when the user has asked for source_cx
             // notification AND hasn't specified that it be sync.
             bool use_handle = !sync && completions_has_event<
                 typename CxStateHere::completions_t,
                 source_cx_event
               >::value
            >
    struct rput_cb_source;

    // rput_cb_operation: rput_cbs_{byref|byval} inherits this to hold
    // operation-completion details.
    template<typename FinalType, typename CxStateHere, typename CxStateRemote,
             bool static_scope = completions_is_event_sync<
                 typename CxStateHere::completions_t,
                 operation_cx_event
               >::value
            >
    struct rput_cb_operation;

    ////////////////////////////////////////////////////////////////////

    // rput_cb_source: Case when the user has an action for source_cx_event.
    // We extend a handle_cb which will fire the completions and transition
    // control to rput_cb_operation.
    template<typename FinalType, typename CxStateHere, typename CxStateRemote>
    struct rput_cb_source<
        FinalType, CxStateHere, CxStateRemote,
        /*sync=*/false, /*use_handle=*/true
      >:
      backend::gasnet::handle_cb {
      
      static constexpr auto source_mode = rma_put_source_mode::handle;

      backend::gasnet::handle_cb* source_cb() {
        return this;
      }
      
      void execute_and_delete(backend::gasnet::handle_cb_successor add_succ) {
        auto *cbs = static_cast<FinalType*>(this);
        
        cbs->state_here.template operator()<source_cx_event>();
        
        add_succ(
          static_cast<
              rput_cb_operation<FinalType, CxStateHere, CxStateRemote>*
            >(cbs)
        );
      }
    };

    // rput_cb_source: Case user does not care about source_cx_event.
    template<typename FinalType, typename CxStateHere, typename CxStateRemote,
             bool sync>
    struct rput_cb_source<
        FinalType, CxStateHere, CxStateRemote,
        sync, /*use_handle=*/false
      > {
      static constexpr auto source_mode =
        sync ? rma_put_source_mode::now : rma_put_source_mode::defer;

      backend::gasnet::handle_cb* source_cb() {
        return nullptr;
      }
    };

    ////////////////////////////////////////////////////////////////////

    // rput_cb_remote: Inherited by rput_cb_operation to handle sending
    // remote rpc events upon operation completion.
    template<typename FinalType, typename CxStateHere, typename CxStateRemote,
             bool has_remote = !CxStateRemote::empty>
    struct rput_cb_remote;
    
    template<typename FinalType, typename CxStateHere, typename CxStateRemote>
    struct rput_cb_remote<FinalType, CxStateHere, CxStateRemote, /*has_remote=*/true> {
      void send_remote() {
        auto *cbs = static_cast<FinalType*>(this);
        
        backend::send_am_master<progress_level::user>(
          cbs->rank_d,
          std::move(cbs->state_remote)
        );
      }
    };
    template<typename FinalType, typename CxStateHere, typename CxStateRemote>
    struct rput_cb_remote<FinalType, CxStateHere, CxStateRemote, /*has_remote=*/false> {
      void send_remote() {/*nop*/}
    };
    
    // rput_cb_operation: Case when the user wants synchronous operation
    // completion.
    template<typename FinalType, typename CxStateHere, typename CxStateRemote>
    struct rput_cb_operation<
        FinalType, CxStateHere, CxStateRemote, /*static_scope=*/true
      >:
      rput_cb_remote<FinalType, CxStateHere, CxStateRemote> {

      static constexpr bool static_scope = true;
      
      void initiate(
          intrank_t rank_d, void *buf_d, const void *buf_s, std::size_t size
        ) {
        auto *cbs = static_cast<FinalType*>(this);
        
        rma_put_b(rank_d, buf_d, buf_s, size);
        
        this->send_remote(); // may nop depending on rput_cb_remote case.
        
        cbs->state_here.template operator()<source_cx_event>();
        cbs->state_here.template operator()<operation_cx_event>();
      }
    };

    // rput_cb_operation: Case where the user does care about the operation
    // actually completing. We inherit from rput_cb_remote which will
    // dispatch to either sending remote completion events or nop.
    template<typename FinalType, typename CxStateHere, typename CxStateRemote>
    struct rput_cb_operation<
        FinalType, CxStateHere, CxStateRemote, /*static_scope=*/false
      >:
      rput_cb_remote<FinalType, CxStateHere, CxStateRemote>,
      backend::gasnet::handle_cb {

      static constexpr bool static_scope = false;
      
      void initiate(
          intrank_t rank, void *buf_d, const void *buf_s, std::size_t size
        ) {
        auto *cbs = static_cast<FinalType*>(this);
        
        rma_put_nb</*source_mode=*/FinalType::source_mode>
          (rank, buf_d, buf_s, size, cbs->source_cb(), this);
      }
      
      void execute_and_delete(backend::gasnet::handle_cb_successor) {
        auto *cbs = static_cast<FinalType*>(this);
        
        this->send_remote(); // may nop depending on rput_cb_remote case.
        
        cbs->state_here.template operator()<operation_cx_event>();
        
        delete cbs; // cleanup object, no more events
      }
    };

    ////////////////////////////////////////////////////////////////////
    // rput_cbs_by{ref|val}: Final classes which hold all completion
    // state and gasnet handles. Inherit from rput_cb_source and
    // rput_cb_operation.
    
    template<typename CxStateHere, typename CxStateRemote>
    struct rput_cbs_byref final:
        rput_cb_source</*FinalType=*/rput_cbs_byref<CxStateHere, CxStateRemote>,
                       CxStateHere, CxStateRemote>,
        rput_cb_operation</*FinalType=*/rput_cbs_byref<CxStateHere, CxStateRemote>,
                          CxStateHere, CxStateRemote> {
      
      intrank_t rank_d;
      CxStateHere state_here;
      CxStateRemote state_remote;

      rput_cbs_byref(intrank_t rank_d, CxStateHere here, CxStateRemote remote):
        rank_d{rank_d},
        state_here{std::move(here)},
        state_remote{std::move(remote)} {
      }
    };
    
    template<typename T, typename CxStateHere, typename CxStateRemote>
    struct rput_cbs_byval final:
        rput_cb_source</*FinalType=*/rput_cbs_byval<T, CxStateHere, CxStateRemote>,
                       CxStateHere, CxStateRemote>,
        rput_cb_operation</*FinalType=*/rput_cbs_byval<T, CxStateHere, CxStateRemote>,
                          CxStateHere, CxStateRemote> {
      
      intrank_t rank_d;
      CxStateHere state_here;
      CxStateRemote state_remote;
      T value;
      
      rput_cbs_byval(
          intrank_t rank_d, CxStateHere here, CxStateRemote remote,
          T value):
        rank_d{rank_d},
        state_here{std::move(here)},
        state_remote{std::move(remote)},
        value{std::move(value)} {
      }
    };
  }
  
  //////////////////////////////////////////////////////////////////////
  // rput
  
  template<typename T,
           typename Cxs = completions<future_cx<operation_cx_event>>>
  typename detail::completions_returner<
      /*EventPredicate=*/detail::event_is_here,
      /*EventValues=*/detail::rput_event_values,
      Cxs
    >::return_t
  rput(T value_s,
       global_ptr<T> gp_d,
       Cxs cxs = completions<future_cx<operation_cx_event>>{{}}) {

    UPCXX_ASSERT_ALWAYS((detail::completions_has_event<Cxs, operation_cx_event>::value));
    
    using cxs_here_t = detail::completions_state<
      /*EventPredicate=*/detail::event_is_here,
      /*EventValues=*/detail::rput_event_values,
      Cxs>;
    using cxs_remote_t = detail::completions_state<
      /*EventPredicate=*/detail::event_is_remote,
      /*EventValues=*/detail::rput_event_values,
      Cxs>;

    detail::rput_cbs_byval<T, cxs_here_t, cxs_remote_t> cbs_static{
      gp_d.rank_,
      cxs_here_t{std::move(cxs)},
      cxs_remote_t{std::move(cxs)},
      std::move(value_s),
    };
    
    auto *cbs = decltype(cbs_static)::static_scope
      ? &cbs_static
      : new decltype(cbs_static){std::move(cbs_static)};
    
    auto returner = detail::completions_returner<
        /*EventPredicate=*/detail::event_is_here,
        /*EventValues=*/detail::rput_event_values,
        Cxs
      >{cbs->state_here};

    cbs->initiate(gp_d.rank_, gp_d.raw_ptr_, &cbs->value, sizeof(T));
    
    return returner();
  }
  
  template<typename T,
           typename Cxs = completions<future_cx<operation_cx_event>>>
  typename detail::completions_returner<
      /*EventPredicate=*/detail::event_is_here,
      /*EventValues=*/detail::rput_event_values,
      Cxs
    >::return_t
  rput(T const *buf_s,
       global_ptr<T> gp_d,
       std::size_t n,
       Cxs cxs = completions<future_cx<operation_cx_event>>{{}}) {
    
    UPCXX_ASSERT_ALWAYS((detail::completions_has_event<Cxs, operation_cx_event>::value));
    
    using cxs_here_t = detail::completions_state<
      /*EventPredicate=*/detail::event_is_here,
      /*EventValues=*/detail::rput_event_values,
      Cxs>;
    using cxs_remote_t = detail::completions_state<
      /*EventPredicate=*/detail::event_is_remote,
      /*EventValues=*/detail::rput_event_values,
      Cxs>;

    detail::rput_cbs_byref<cxs_here_t, cxs_remote_t> cbs_static{
      gp_d.rank_,
      cxs_here_t{std::move(cxs)},
      cxs_remote_t{std::move(cxs)}
    };

    auto *cbs = decltype(cbs_static)::static_scope
      ? &cbs_static
      : new decltype(cbs_static){std::move(cbs_static)};
    
    auto returner = detail::completions_returner<
        /*EventPredicate=*/detail::event_is_here,
        /*EventValues=*/detail::rput_event_values,
        Cxs
      >{cbs->state_here};
    
    cbs->initiate(gp_d.rank_, gp_d.raw_ptr_, buf_s, n*sizeof(T));
    
    return returner();
  }
}
#endif
