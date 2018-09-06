#ifndef _fdec460f_0bc2_4c14_b88c_c85d76235cd3
#define _fdec460f_0bc2_4c14_b88c_c85d76235cd3

#include <upcxx/backend.hpp>
#include <upcxx/completion.hpp>
#include <upcxx/rpc.hpp>
#include <upcxx/team.hpp>

#include <type_traits>

namespace upcxx {
  namespace detail {
    /* The function objects for `op_fast_add` etc are actually all instantiations
     * of `op_wrap<OpFn>` where `OpFn` is the type that actually implements the
     * `operator()` logic. We wrap types this way to make it easy to pattern match
     * our function objects in template specializations. We also carry a template
     * argument `fast_demanded` to indicate if this operator should statically
     * assert it's being used in a *possibly* offloadable way.
     */
    template<typename OpFn, bool fast_demanded>
    struct op_wrap: OpFn {
      // inherit `operator()` from `OpFn`
    };
  }
  
  /* `detail::opfn_[add|...]` is the function object which actually implements
   * `operator()` and is used as the value for `OpFn` in `op_wrap`.
   */
  #define UPCXX_INFIX_OP(tok, name, integral_only1) \
    namespace detail {\
      struct opfn_##name {\
        static constexpr bool integral_only = integral_only1;\
        template<typename T, typename Tb>\
        T operator()(T a, Tb &&b) const {\
          a tok##= std::forward<Tb>(b);\
          return static_cast<T&&>(a);\
        }\
      };\
    }\
    constexpr detail::op_wrap<detail::opfn_##name, /*fast_demanded=*/false> op_##name;\
    constexpr detail::op_wrap<detail::opfn_##name, /*fast_demanded=*/true> op_fast_##name;
  
  UPCXX_INFIX_OP(+, add, false)
  UPCXX_INFIX_OP(*, mul, false)
  UPCXX_INFIX_OP(&, bit_and, true)
  UPCXX_INFIX_OP(|, bit_or, true)
  UPCXX_INFIX_OP(^, bit_xor, true)
  #undef UPCXX_INFIX_OP
  
  namespace detail {
    template<bool min_not_max>
    struct opfn_min_not_max {
      static constexpr bool integral_only = false;
      
      template<typename T>
      T&& operator()(T &&a, T &&b) const {
        return (min_not_max ? a < b : a > b) ? static_cast<T&&>(a) : static_cast<T&&>(b);
      }
      template<typename T>
      T const& operator()(T const &a, T const &b) const {
        return (min_not_max ? a < b : a > b) ? a : b;
      }
    };
  }
  constexpr detail::op_wrap<detail::opfn_min_not_max<true>, /*fast_demanded=*/false> op_min;
  constexpr detail::op_wrap<detail::opfn_min_not_max<false>, /*fast_demanded=*/false> op_max;
  constexpr detail::op_wrap<detail::opfn_min_not_max<true>, /*fast_demanded=*/true> op_fast_min;
  constexpr detail::op_wrap<detail::opfn_min_not_max<false>, /*fast_demanded=*/true> op_fast_max;
  
  namespace detail {
    /* `detail::reduce_op_has_fast<Op,T>` matches Op and T and reports to bools:
     * whether the combo is offloadable in theory (possibly) and in practice (actually).
     */ 
    // default for unknown Op,T is both false
    template<typename Op, typename T>
    struct reduce_op_has_fast {
      static constexpr bool possibly = false;
      static constexpr bool actually = false;
    };
    // match where `Op = op_wrap<OpFn,...>`
    template<typename OpFn, bool fast_demanded, typename T>
    struct reduce_op_has_fast<op_wrap<OpFn, fast_demanded>, T> {
      static constexpr bool possibly = std::is_integral<T>::value || (
          !OpFn::integral_only && std::is_floating_point<T>::value
        );
      
      static constexpr bool actually = possibly && (8*sizeof(T)==32 || 8*sizeof(T)==64);
    };
    
    ////////////////////////////////////////////////////////////////////////////
    // The following is support for reduce_op_best_id<Op,T>:
    
    /* We use const global variables to hold GEX constants. The ".cpp" file
     * defines these since it pulls in GEX headers, we only declare them.
     */
    struct reduce_op_slow_op_id {
      static const std::uintptr_t op_id; // = GEX_OP_USER
    };
    struct reduce_op_slow_ty_id {
      static const std::uintptr_t ty_id; // = GEX_DT_USER
    };
    
    template<typename Op, typename T>
    struct reduce_op_slow_id:
      reduce_op_slow_op_id,
      reduce_op_slow_ty_id {
      
      // The vectorized user provided function for GEX_OP_USER
      static void op_vecfn(const void *arg1, void *arg2_and_out, std::size_t n, const void *data) {
        T const *a = static_cast<T const*>(arg1);
        T *b_out = static_cast<T*>(arg2_and_out);
        Op const *op = static_cast<Op const*>(data);
        
        while(n--) {
          *b_out = (*op)(*a, *b_out);
          a++;
          b_out++;
        }
      }
    };
    
    // Parameterize global const names for GEX_OP_*** using the `opfn_***` types.
    template<typename OpFn>
    struct reduce_op_fast_op_id {
      static const std::uintptr_t op_id; // the GEX OP name for this operation (ex: op_id = GEX_OP_ADD when OpFn = opfn_add)
      static constexpr nullptr_t op_vecfn = nullptr; // No vectorized user function necessary
    };
    // reduce_op_fast_op_id implicitly unwraps `op_wrap<OpFn,...>`
    template<typename OpFn, bool fast_demanded>
    struct reduce_op_fast_op_id<op_wrap<OpFn,fast_demanded>>:
      reduce_op_fast_op_id<OpFn> {
    };
    
    // Parameterize global const names for GEX_DT_*** using the measurements of the type.
    template<int bits, bool is_signed>
    struct reduce_op_fast_ty_id_integral {
      static const std::uintptr_t ty_id; // the GEX DT name for this type
    };
    template<int bits>
    struct reduce_op_fast_ty_id_floating {
      static const std::uintptr_t ty_id; // the GEX DT name for this type
    };
    
    template<typename T,
             bool is_integral = std::is_integral<T>::value,
             bool is_floating = std::is_floating_point<T>::value>
    struct reduce_op_fast_ty_id: reduce_op_slow_ty_id {};
    
    template<typename T>
    struct reduce_op_fast_ty_id<T,
        /*is_integral=*/true,
        /*is_floating=*/false
      >: reduce_op_fast_ty_id_integral</*bits=*/8*sizeof(T), std::is_signed<T>::value> {
    };
    template<typename T>
    struct reduce_op_fast_ty_id<T,
        /*is_integral=*/false,
        /*is_floating=*/true
      >: reduce_op_fast_ty_id_floating</*bits=*/8*sizeof(T)> {
    };
    
    ////////////////////////////////////////////////////////////////////////////
    // reduce_op_best_id<Op,T>:
    
    /* `reduce_op_best_id<Op,T>` produces the following members:
     *   static const uintptr_t op_id; // the GEX_OP_***
     *   static const void(*op_vecfn)(...); // function pointer for GEX_OP_USER
     *   static const uintptr_t ty_id; // the GEX_DT_***
     */
    template<typename Op, typename T,
             bool possibly_fast = reduce_op_has_fast<Op,T>::possibly,
             bool actually_fast = reduce_op_has_fast<Op,T>::actually>
    struct reduce_op_best_id;
    
    template<typename Op, typename T>
    struct reduce_op_best_id<Op, T, /*possibly=*/true, /*actually=*/true>:
      reduce_op_fast_op_id<Op>,
      reduce_op_fast_ty_id<T> {
    };
    template<typename Op, typename T>
    struct reduce_op_best_id<Op, T, /*possibly=*/true, /*actually=*/false>:
      reduce_op_slow_id<Op,T> {
    };
    template<typename OpFn, bool fast_demanded, typename T>
    struct reduce_op_best_id<op_wrap<OpFn,fast_demanded>, T, /*possibly=*/false, /*actually=*/false>:
      reduce_op_slow_id<op_wrap<OpFn,fast_demanded>, T> {
      static_assert(!fast_demanded, "upcxx::op_fast_*** cannot be applied to this datatype.");
    };
    template<typename Op, typename T>
    struct reduce_op_best_id<Op, T, /*possibly=*/false, /*actually=*/false>:
      reduce_op_slow_id<Op,T> {
    };
    
    ////////////////////////////////////////////////////////////////////////////
    // Calls out to gex_Coll_ReduceToOneNB
    
    void reduce_one_trivial(
        team &tm, intrank_t root,
        const void *src, void *dst,
        std::size_t elt_sz, std::size_t elt_n,
        std::uintptr_t ty_id,
        std::uintptr_t op_id,
        void(*op_vecfn)(const void*, void*, std::size_t, const void*),
        void *op_data,
        backend::gasnet::handle_cb *cb
      );
    
    ////////////////////////////////////////////////////////////////////////////
    // Non-GEX reduction implementation. Used for nontrivial types T.
    
    template<typename T, typename Op, bool one_not_all>
    struct reduce_state {
      intrank_t root; // root rank for reduction tree
      int incoming; // number of ranks sending us a contribution, counts down to zero.
      T accum; // accumulates contributions from local user and other ranks.
      promise<T> answer;
      
      template<typename T1>
      static future<T> contribute(team&, intrank_t root, digest id, Op const &op, T1 &&value);
    };
  }
  
  //////////////////////////////////////////////////////////////////////////////
  // upcxx::reduce_one
  
  template<typename T1, typename BinaryOp,
           typename Cxs = completions<future_cx<operation_cx_event>>,
           typename T = typename std::decay<T1>::type>
  future<T> reduce_one(
      T1 &&value, BinaryOp op, intrank_t root,
      team &tm = upcxx::world(),
      completions<future_cx<operation_cx_event>> cxs_ignored = {{}}
    ) {
    #if 1
      struct my_cb final: backend::gasnet::handle_cb {
        T in_out;
        BinaryOp op;
        promise<T> pro;
        
        my_cb(T1 &&in, BinaryOp op):
          in_out(std::forward<T1>(in)),
          op(std::move(op)) {
        }
        
        void execute_and_delete(backend::gasnet::handle_cb_successor) override {
          backend::fulfill_during_user(
              std::move(pro), std::tuple<T>(std::move(in_out)),
              backend::master
            );
          delete this;
        }
      };
      
      my_cb *cb = new my_cb(std::forward<T1>(value), std::move(op));
      future<T> ans = cb->pro.get_future();
      
      detail::reduce_one_trivial(
          tm, root,
          &cb->in_out, &cb->in_out, sizeof(T), 1,
          detail::reduce_op_best_id<BinaryOp,T>::ty_id,
          detail::reduce_op_best_id<BinaryOp,T>::op_id,
          detail::reduce_op_best_id<BinaryOp,T>::op_vecfn,
          (void*)&cb->op,
          cb
        );
      
      return ans;
    #else
      digest id = tm.next_collective_id(detail::internal_only());
      return detail::reduce_state<T,BinaryOp,/*one_not_all=*/true>::contribute(tm, root, id, op, std::forward<T1>(value));
    #endif
  }
  
  template<typename T, typename BinaryOp,
           typename Cxs = completions<future_cx<operation_cx_event>>>
  future<> reduce_one(
      T const *src, T *dst, std::size_t n,
      BinaryOp op, intrank_t root,
      team &tm = upcxx::world(),
      completions<future_cx<operation_cx_event>> cxs_ignored = {{}}
    ) {
    
    struct my_cb final: backend::gasnet::handle_cb {
      BinaryOp op;
      promise<> pro;
      
      my_cb(BinaryOp op):
        op(std::move(op)) {
      }
      
      void execute_and_delete(backend::gasnet::handle_cb_successor) override {
        backend::fulfill_during_user(std::move(pro), std::tuple<>(), backend::master);
        delete this;
      }
    };
    
    my_cb *cb = new my_cb(std::move(op));
    future<> ans = cb->pro.get_future();
    
    detail::reduce_one_trivial(
        tm, root,
        src, dst, sizeof(T), n,
        detail::reduce_op_best_id<BinaryOp,T>::ty_id,
        detail::reduce_op_best_id<BinaryOp,T>::op_id,
        detail::reduce_op_best_id<BinaryOp,T>::op_vecfn,
        (void*)&cb->op,
        cb
      );
    
    return ans;
  }
  
  //////////////////////////////////////////////////////////////////////////////
  // upcxx::reduce_one_nontrivial
  
  template<typename T1, typename BinaryOp,
           typename Cxs = completions<future_cx<operation_cx_event>>,
           typename T = typename std::decay<T1>::type>
  future<T> reduce_one_nontrivial(
      T1 &&value, BinaryOp op, intrank_t root,
      team &tm = upcxx::world(),
      completions<future_cx<operation_cx_event>> cxs_ignored = {{}}
    ) {
    digest id = tm.next_collective_id(detail::internal_only());
    return detail::reduce_state<T,BinaryOp,/*one_not_all=*/true>::contribute(tm, root, id, op, std::forward<T1>(value));
  }
  
  //////////////////////////////////////////////////////////////////////////////
  // upcxx::reduce_all
  
  template<typename T1, typename BinaryOp,
           typename Cxs = completions<future_cx<operation_cx_event>>,
           typename T = typename std::decay<T1>::type>
  future<T> reduce_all(
      T1 &&value, BinaryOp op,
      team &tm = upcxx::world(),
      completions<future_cx<operation_cx_event>> cxs_ignored = {{}}
    ) {
    static_assert(
      upcxx::is_definitely_trivially_serializable<T>::value,
      "`upcxx::reduce_all<T>` only permitted for DefinitelyTriviallySerialize T. "
      "Consider using `upcxx::reduce_all_nontrivial<T>` instead."
    );
    digest id = tm.next_collective_id(detail::internal_only());
    intrank_t root = id.w0 % tm.rank_n();
    return detail::reduce_state<T,BinaryOp,/*one_not_all=*/false>::contribute(tm, root, id, op, std::forward<T1>(value));
  }
  
  //////////////////////////////////////////////////////////////////////////////
  // upcxx::reduce_all_nontrivial
  
  template<typename T1, typename BinaryOp,
           typename Cxs = completions<future_cx<operation_cx_event>>,
           typename T = typename std::decay<T1>::type>
  future<T> reduce_all_nontrivial(
      T1 &&value, BinaryOp op,
      team &tm = upcxx::world(),
      completions<future_cx<operation_cx_event>> cxs_ignored = completions<future_cx<operation_cx_event>>{{}}
    ) {
    digest id = tm.next_collective_id(detail::internal_only());
    intrank_t root = id.w0 % tm.rank_n();
    return detail::reduce_state<T,BinaryOp,/*one_not_all=*/false>::contribute(tm, root, id, op, std::forward<T1>(value));
  }
  
  //////////////////////////////////////////////////////////////////////////////
  // reduce_state::contribute
  
  namespace detail {
    template<typename T, typename Op, bool one_not_all>
    template<typename T1>
    future<T> reduce_state<T,Op,one_not_all>::contribute(
        team &tm, intrank_t root, digest id, Op const &op, T1 &&value
      ) {
      
      UPCXX_ASSERT(backend::master.active_with_caller());
      
      intrank_t rank_n = tm.rank_n();
      
      // Use rank indexing scheme where the root is zero.
      intrank_t rank_me = tm.rank_me() - root;
      rank_me += rank_me < 0 ? rank_n : 0;
      
      auto it_and_inserted = detail::registry.insert({id, nullptr});
      reduce_state *state;
      
      if(it_and_inserted.second) {
        // `id` didn't exist in registry so we're first to contribute.
        
        // Given the parent function which flips the least significant one-bit
        //   parent(r) = r & (r-1)
        // Count number of bitpatterns in [0,rank_n) which when asked for
        // their parent, would produce us (rank_me).
        int incoming = 0;
        while(true) {
          intrank_t child = rank_me | (intrank_t(1)<<incoming);
          if(child == rank_me || rank_n <= child)
            break;
          incoming += 1;
        }
        incoming += 1; // add one for this rank
        
        state = new reduce_state{root, incoming, std::forward<T1>(value)};
        it_and_inserted.first->second = state;
      }
      else {
        // Contributions already exist, just add ours to it.
        state = static_cast<reduce_state*>(it_and_inserted.first->second);
        state->accum = op(std::move(state->accum), std::forward<T1>(value));
      }
      
      future<T> ans = state->answer.get_future();
      
      if(0 == --state->incoming) {
        // We have all of our expected contributions.
        if(rank_me == 0) {
          // We are root, time to broadcast result.
          auto bound = upcxx::bind([=](T &value) {
                auto it = detail::registry.find(id);
                reduce_state *me = static_cast<reduce_state*>(it->second);
                detail::registry.erase(it);
                
                backend::fulfill_during_user(std::move(me->answer), std::tuple<T>(std::move(value)));
                delete me;
              },
              std::move(state->accum)
            );
          
          if(!one_not_all)
            backend::bcast_am_master<progress_level::user>(tm, bound);
          
          backend::fulfill_during_user(
              std::move(state->answer),
              std::tuple<T>(std::move(std::get<0>(bound.b_))),
              backend::master
            );
          delete state;
          detail::registry.erase(id);
        }
        else {
          // Send our result to parent. Parent rank's index is the same as ours
          // except with the least significant one-bit set to zero.
          intrank_t parent = rank_me & (rank_me-1);
          // Translate back to team's indexing scheme
          parent += root;
          parent -= parent >= rank_n ? rank_n : 0;
          
          team_id tm_id = tm.id();
          
          backend::template send_am_master<progress_level::internal>(
            tm, parent,
            upcxx::bind(
              [=](T &value, decltype(detail::globalize_fnptr(std::declval<Op>())) const &op) {
                reduce_state::contribute(tm_id.here(), root, id, op, static_cast<T&&>(value));
              },
              state->accum, detail::globalize_fnptr(op)
            )
          );
          
          if(one_not_all) {
            backend::fulfill_during_user(
                std::move(state->answer),
                // state->accum has been moved-out from, so this is really bogus,
                // but still correct.
                std::tuple<T>(std::move(state->accum)),
                backend::master
              );
            delete state;
            detail::registry.erase(id);
          }
        }
      }
      
      return ans;
    }
  }
}
#endif
