#ifndef _4fd2caba_e406_4f0e_ab34_0e0224ec36a5
#define _4fd2caba_e406_4f0e_ab34_0e0224ec36a5

#include <vector>
#include <upcxx/backend.hpp>
#include <upcxx/completion.hpp>
#include <upcxx/global_ptr.hpp>

namespace upcxx {
  namespace atomic {
    // All supported atomic operations.
    enum AOP: int { GET, SET, ADD, FADD, SUB, FSUB, INC, FINC, DEC, FDEC, CSWAP };
  }

  namespace detail {
    template<typename T>
    void gex_AD_OpNB(uintptr_t, T*, upcxx::global_ptr<T>, atomic::AOP, int, T, T, std::memory_order,
                     backend::gasnet::handle_cb*);
#define SET_GEX_OP_PROTOTYPE(T) \
    template<> \
    void gex_AD_OpNB(uintptr_t ad, T* p, upcxx::global_ptr<T>, atomic::AOP opcode, int allowed_ops, \
                     T val1, T val2, std::memory_order order, backend::gasnet::handle_cb *cb);
    SET_GEX_OP_PROTOTYPE(int32_t);
    SET_GEX_OP_PROTOTYPE(int64_t);
    SET_GEX_OP_PROTOTYPE(uint32_t);
    SET_GEX_OP_PROTOTYPE(uint64_t);

    // event values for non-fetching operations
    struct nofetch_aop_event_values {
      template<typename Event>
      using tuple_t = std::tuple<>;
    };
    // event values for fetching operations
    template<typename T>
    struct fetch_aop_event_values {
      template<typename Event>
      using tuple_t = typename std::conditional<
          std::is_same<Event, operation_cx_event>::value,
          std::tuple<T>,
          std::tuple<>
        >::type;
    };

    // The class that handles the gasnet event. This is for non-fetching ops.
    // Must be declared final for the 'delete this' call.
    template<typename CxStateHere>
    struct nofetch_op_cb final: backend::gasnet::handle_cb {
      CxStateHere state_here;

      nofetch_op_cb(CxStateHere state_here) : state_here{std::move(state_here)} {}

      // The callback executed upon event completion.
      void execute_and_delete(backend::gasnet::handle_cb_successor) {
        this->state_here.template operator()<operation_cx_event>();
        delete this;
      }
    };
    // The class that handles the gasnet event. For fetching ops.
    template<typename T, typename CxStateHere>
    struct fetch_op_cb final: backend::gasnet::handle_cb {
      CxStateHere state_here;
      T result;

      fetch_op_cb(CxStateHere state_here) : state_here{std::move(state_here)} {}

      void execute_and_delete(backend::gasnet::handle_cb_successor) {
        this->state_here.template operator()<operation_cx_event>(std::move(result));
        delete this;
      }
    };

  }

  namespace atomic {
    // convenience declarations
    template<typename T, typename Cxs>
    using FETCH_RTYPE = typename detail::completions_returner<detail::event_is_here,
        detail::fetch_aop_event_values<T>, Cxs>::return_t;
    template<typename Cxs>
    using NOFETCH_RTYPE = typename detail::completions_returner<detail::event_is_here,
        detail::nofetch_aop_event_values, Cxs>::return_t;
    using FUTURE_CX = completions<future_cx<operation_cx_event> >;

    // Atomic domain for an ?int*_t type.
    template<typename T>
    class domain {
      private:
        // The opaque gasnet atomic domain handle.
        uintptr_t gex_ad;
        // The or'd value for all the atomic operations.
        int gex_ops;

        // non-fetching operations
        template<AOP aop, typename Cxs>
        NOFETCH_RTYPE<Cxs> _nofetch_op(global_ptr<T> gptr, std::memory_order order, T val1, T val2,
                                       Cxs cxs) {
          UPCXX_ASSERT_ALWAYS((detail::completions_has_event<Cxs, operation_cx_event>::value));
          // we only have local completion, not remote
          using cxs_here_t = detail::completions_state<detail::event_is_here,
              detail::nofetch_aop_event_values, Cxs>;
          // Create the callback object..
          auto *cb = new detail::nofetch_op_cb<cxs_here_t>{cxs_here_t{std::move(cxs)}};
          auto returner = detail::completions_returner<detail::event_is_here,
              detail::nofetch_aop_event_values, Cxs>{cb->state_here};
          // execute the backend gasnet function
          upcxx::detail::gex_AD_OpNB<T>(gex_ad, nullptr, gptr, aop, gex_ops, val1, val2, order, cb);
          return returner();
        }

        // fetching operations
        template<AOP aop, typename Cxs>
        FETCH_RTYPE<T, Cxs> _fetch_op(global_ptr<T> gptr, std::memory_order order, T val1, T val2,
                                      Cxs cxs) {
          UPCXX_ASSERT_ALWAYS((detail::completions_has_event<Cxs, operation_cx_event>::value));
          // we only have local completion, not remote
          using cxs_here_t = detail::completions_state<detail::event_is_here,
              detail::fetch_aop_event_values<T>, Cxs>;
          // Create the callback object..
          auto *cb = new detail::fetch_op_cb<T, cxs_here_t>{cxs_here_t{std::move(cxs)}};
          auto returner = detail::completions_returner<detail::event_is_here,
              detail::fetch_aop_event_values<T>, Cxs>{cb->state_here};
          // execute the backend gasnet function
          upcxx::detail::gex_AD_OpNB<T>(gex_ad, &cb->result, gptr, aop, gex_ops, val1, val2,
              order, cb);
          return returner();
        }

      public:
        // The constructor takes a vector of operations. Currently, flags is unsupported.
        domain(std::vector<int> ops, int flags = 0);
        ~domain();

        // Generic fetching atomic operation. This can take 0, 1 or 2 operands.
        template<AOP aop, typename Cxs = FUTURE_CX>
        FETCH_RTYPE<T, Cxs> fop(global_ptr<T> gptr, std::memory_order order, T val1 = 0, T val2 = 0,
                                Cxs cxs = FUTURE_CX{{}}) {
          return _fetch_op<aop, Cxs>(gptr, order, val1, val2, cxs);
        }

        // Generic non-fetching atomic operation. This can take 0, 1 or 2 operands.
        template<AOP aop, typename Cxs = FUTURE_CX>
        NOFETCH_RTYPE<Cxs> op(global_ptr<T> gptr, std::memory_order order, T val1 = 0, T val2 = 0,
                              Cxs cxs = FUTURE_CX{{}}) {
          return _nofetch_op<aop, Cxs>(gptr, order, val1, val2, cxs);
        }

        // Convenience functions for all the operations.
        template<typename Cxs = FUTURE_CX>
        NOFETCH_RTYPE<Cxs> set(global_ptr<T> gptr, T val,
                               std::memory_order order = std::memory_order_relaxed,
                               Cxs cxs = FUTURE_CX{{}}) {
          return _nofetch_op<SET, Cxs>(gptr, order, val, (T)0, cxs);
        }
        template<typename Cxs = FUTURE_CX>
        FETCH_RTYPE<T, Cxs> get(global_ptr<T> gptr,
                                std::memory_order order = std::memory_order_relaxed,
                                Cxs cxs = FUTURE_CX{{}}) {
          return _fetch_op<GET>(gptr, order, (T)0, (T)0, cxs);
        }
        template<typename Cxs = FUTURE_CX>
        NOFETCH_RTYPE<Cxs> inc(global_ptr<T> gptr,
                               std::memory_order order = std::memory_order_relaxed,
                               Cxs cxs = FUTURE_CX{{}}) {
          return _nofetch_op<INC>(gptr, order, (T)0, (T)0, cxs);
        }
        template<typename Cxs = FUTURE_CX>
        NOFETCH_RTYPE<Cxs> dec(global_ptr<T> gptr,
                               std::memory_order order = std::memory_order_relaxed,
                               Cxs cxs = FUTURE_CX{{}}) {
          return _nofetch_op<DEC>(gptr, order, (T)0, (T)0, cxs);
        }
        template<typename Cxs = FUTURE_CX>
        FETCH_RTYPE<T, Cxs> finc(global_ptr<T> gptr,
                                 std::memory_order order = std::memory_order_relaxed,
                                 Cxs cxs = FUTURE_CX{{}}) {
          return _fetch_op<FINC>(gptr, order, (T)0, (T)0, cxs);
        }
        template<typename Cxs = FUTURE_CX>
        FETCH_RTYPE<T, Cxs> fdec(global_ptr<T> gptr,
                                 std::memory_order order = std::memory_order_relaxed,
                                 Cxs cxs = FUTURE_CX{{}}) {
          return _fetch_op<FDEC>(gptr, order, (T)0, (T)0, cxs);
        }
        template<typename Cxs = FUTURE_CX>
        NOFETCH_RTYPE<Cxs> add(global_ptr<T> gptr, T val1,
                               std::memory_order order = std::memory_order_relaxed,
                               Cxs cxs = FUTURE_CX{{}}) {
          return _nofetch_op<ADD>(gptr, order, val1, (T)0, cxs);
        }
        template<typename Cxs = FUTURE_CX>
        NOFETCH_RTYPE<Cxs> sub(global_ptr<T> gptr, T val1,
                               std::memory_order order = std::memory_order_relaxed,
                               Cxs cxs = FUTURE_CX{{}}) {
          return _nofetch_op<SUB>(gptr, order, val1, (T)0, cxs);
        }
        template<typename Cxs = FUTURE_CX>
        FETCH_RTYPE<T, Cxs> fadd(global_ptr<T> gptr, T val,
                                 std::memory_order order = std::memory_order_relaxed,
                                 Cxs cxs = FUTURE_CX{{}}) {
          return _fetch_op<FADD>(gptr, order, val, (T)0, cxs);
        }
        template<typename Cxs = FUTURE_CX>
        FETCH_RTYPE<T, Cxs> fsub(global_ptr<T> gptr, T val,
                                 std::memory_order order = std::memory_order_relaxed,
                                 Cxs cxs = FUTURE_CX{{}}) {
          return _fetch_op<FSUB>(gptr, order, val, (T)0, cxs);
        }
        template<typename Cxs = FUTURE_CX>
        FETCH_RTYPE<T, Cxs> cswap(global_ptr<T> gptr, T val1, T val2,
                                  std::memory_order order = std::memory_order_relaxed,
                                  Cxs cxs = FUTURE_CX{{}}) {
          return _fetch_op<CSWAP>(gptr, order, val1, val2, cxs);
        }
    };
  } // namespace atomic
} // namespace upcxx

#endif
