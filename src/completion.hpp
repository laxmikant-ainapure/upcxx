#ifndef _96368972_b5ed_4e48_ac4f_8c868279e3dd
#define _96368972_b5ed_4e48_ac4f_8c868279e3dd

#include <upcxx/bind.hpp>
#include <upcxx/future.hpp>

namespace upcxx {
  struct nil_cx {};
  
  template<typename Fn>
  struct rpc_cx {
    Fn fn_;
    rpc_cx(Fn fn): fn_{std::move(fn)} {}
  };
  
  template<int ordinal>
  struct future_cx {};
  
  template<typename ...T>
  struct promise_cx {
    promise<T...> &pro_;
  };
  
  //////////////////////////////////////////////////////////////////////
  
  namespace detail {
    template<typename A, typename B, int B_ord_bump>
    struct disjoin_cx;
    
    template<typename A, int B_ord_bump>
    struct disjoin_cx<A, nil_cx, B_ord_bump> {
      using type = A;
      
      A&& operator()(A &&a, nil_cx &&b) {
        return std::move(a);
      }
    };
    template<typename B, int B_ord_bump>
    struct disjoin_cx<nil_cx, B, B_ord_bump> {
      using type = B;
      
      B&& operator()(nil_cx &&a, B &&b) {
        return std::move(b);
      }
    };
    template<int B_ord_old, int B_ord_bump>
    struct disjoin_cx<nil_cx, future_cx<B_ord_old>, B_ord_bump> {
      using type = future_cx<B_ord_old + B_ord_bump>;
      
      future_cx<B_ord_old + B_ord_bump> operator()(nil_cx &&a, future_cx<B_ord_old> &&b) {
        return {};
      }
    };
    template<int B_ord0>
    struct disjoin_cx<nil_cx, nil_cx, B_ord0> {
      using type = nil_cx;
      
      nil_cx operator()(nil_cx &&a, nil_cx &&b) {
        return {};
      }
    };
    
    template<typename Cx>
    struct is_future_cx: std::false_type {};
    template<int ordinal>
    struct is_future_cx<future_cx<ordinal>>: std::true_type {};
  }
  
  //////////////////////////////////////////////////////////////////////
  
  template<typename SourceCx, typename RemoteCx, typename OpxnCx>
  struct completions {
    SourceCx source;
    RemoteCx remote;
    OpxnCx operxn;
    
    static constexpr int future_n =
      (detail::is_future_cx<SourceCx>::value ? 1 : 0) +
      (detail::is_future_cx<OpxnCx>::value ? 1 : 0);
  };
  
  template<typename AS, typename AR, typename AO,
           typename BS, typename BR, typename BO>
  completions<
      typename detail::disjoin_cx<AS, BS, completions<AS, AR, AO>::future_n>::type,
      typename detail::disjoin_cx<AR, BR, completions<AS, AR, AO>::future_n>::type,
      typename detail::disjoin_cx<AO, BO, completions<AS, AR, AO>::future_n>::type
    >
    operator|(
      completions<AS, AR, AO> a,
      completions<BS, BR, BO> b
    ) {
    
    static constexpr int B_ord_bump = completions<AS, AR, AO>::future_n;
    
    return completions<
        typename detail::disjoin_cx<AS, BS, B_ord_bump>::type,
        typename detail::disjoin_cx<AR, BR, B_ord_bump>::type,
        typename detail::disjoin_cx<AO, BO, B_ord_bump>::type
      >{
      detail::disjoin_cx<AS, BS, B_ord_bump>()(std::move(a.source), std::move(b.source)),
      detail::disjoin_cx<AR, BR, B_ord_bump>()(std::move(a.remote), std::move(b.remote)),
      detail::disjoin_cx<AO, BO, B_ord_bump>()(std::move(a.operxn), std::move(b.operxn))
    };
  }
  
  //////////////////////////////////////////////////////////////////////
  
  constexpr completions<future_cx<0>, nil_cx, nil_cx> source_cx_as_future = {};
  constexpr completions<nil_cx, nil_cx, future_cx<0>> operxn_cx_as_future = {};
  
  template<typename ...T>
  completions<promise_cx<T...>, nil_cx, nil_cx> source_cx_as_promise(promise<T...> &pro) {
    return {{pro}, {}, {}};
  }
  
  template<typename ...T>
  completions<nil_cx, nil_cx, promise_cx<T...>> operxn_cx_as_promise(promise<T...> &pro) {
    return {{}, {}, {pro}};
  }
  
  template<typename Fn, typename ...Args>
  auto remote_cx_as_rpc(Fn &&fn, Args &&...args)
    -> completions<
      nil_cx,
      rpc_cx<decltype(upcxx::bind(std::forward<Fn>(fn), std::forward<Args>(args)...))>,
      nil_cx
    > {
    return {{}, {upcxx::bind(std::forward<Fn>(fn), std::forward<Args>(args)...)}, {}};
  }
}
#endif

