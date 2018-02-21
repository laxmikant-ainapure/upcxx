#ifndef _4fd2caba_e406_4f0e_ab34_0e0224ec36a5
#define _4fd2caba_e406_4f0e_ab34_0e0224ec36a5

#include <vector>
#include <upcxx/backend.hpp>
#include <upcxx/completion.hpp>
#include <upcxx/global_ptr.hpp>

namespace upcxx {
  namespace detail {
    template<typename T>
    void gex_AD_OpNB(uintptr_t, T*, upcxx::global_ptr<T>, int, T, T, int,
                     backend::gasnet::handle_cb*);
#define SET_GEX_OP_PROTOTYPE(T) \
    template<> \
    void gex_AD_OpNB(uintptr_t ad, T* p, upcxx::global_ptr<T>, int opcode, \
                     T val1, T val2, int flags, backend::gasnet::handle_cb *cb);
    SET_GEX_OP_PROTOTYPE(int32_t);
    SET_GEX_OP_PROTOTYPE(int64_t);
    SET_GEX_OP_PROTOTYPE(uint32_t);
    SET_GEX_OP_PROTOTYPE(uint64_t);

    int get_gex_flags(std::memory_order order);
    int to_gex_op(int opcode);

    template<typename T>
    struct aop_event_values {
      template<typename Event>
      using tuple_t = typename std::conditional<
          std::is_same<Event, operation_cx_event>::value,
          std::tuple<T>,
          std::tuple<>
        >::type;
    };

    // The class that handles the gasnet event.
    // Must be declared final for the 'delete this' call.
    template<typename T, typename CxStateHere>
    struct op_cb final: backend::gasnet::handle_cb {

      CxStateHere state_here;
      T result;

      op_cb(CxStateHere state_here) : state_here{std::move(state_here)} {}

      // The callback executed upon event completion.
      void execute_and_delete(backend::gasnet::handle_cb_successor) {
        this->state_here.template operator()<operation_cx_event>(std::move(result));
        delete this;
      }
    };
  }

  namespace atomic {
    // All supported atomic operations.
    enum AOP: int { GET, SET, ADD, FADD, SUB, FSUB, INC, FINC, DEC, FDEC, CSWAP };
    static std::string atomic_op_str[] = {
      "GET", "SET", "ADD", "FADD", "SUB", "FSUB", "INC", "FINC", "DEC", "FDEC", "CSWAP" };

    // Atomic domain for an ?int*_t type.
    template<typename T>
    class domain {
      private:
        // The opaque gasnet atomic domain handle.
        uintptr_t gex_ad;
        // The or'd value for all the atomic operations.
        int gex_ops;
      public:
        // The constructor takes a vector of operations. Currently, flags is unsupported.
        domain(std::vector<int> ops, int flags = 0);
        ~domain();

        // Generic atomic operation. This can take 0, 1 or 2 operands.
        template<typename Cxs = completions<future_cx<operation_cx_event> > >
        typename detail::completions_returner<detail::event_is_here,
            detail::aop_event_values<T>, Cxs>::return_t
        op(AOP aop, global_ptr<T> gptr, std::memory_order order, T val1 = 0, T val2 = 0,
           Cxs cxs = completions<future_cx<operation_cx_event> >{ {} }) {

          UPCXX_ASSERT_ALWAYS((detail::completions_has_event<Cxs, operation_cx_event>::value));

          // get our gasnet operation
          int gex_op = upcxx::detail::to_gex_op(aop);
          // Fail if attempting to use an atomic operation not part of this domain.
          UPCXX_ASSERT(gex_op & gex_ops,
                       "Atomic operation " << atomic_op_str[aop] << " not included in domain\n");
          // select the appropriate flags for the memory order
          int flags = upcxx::detail::get_gex_flags(order);

          // we only have local completion, not remote
          using cxs_here_t = detail::completions_state<detail::event_is_here,
              detail::aop_event_values<T>, Cxs>;

          // Create the callback object..
          auto *cb = new detail::op_cb<T, cxs_here_t>{cxs_here_t{std::move(cxs)}};

          auto returner = detail::completions_returner<detail::event_is_here,
              detail::aop_event_values<T>, Cxs>{cb->state_here};

          // execute the backend gasnet function
          upcxx::detail::gex_AD_OpNB<T>(gex_ad, &cb->result, gptr, gex_op,
              val1, val2, flags, cb);

          return returner();
        }

        // Convenience functions for all the operations.
        future<> set(global_ptr<T> gptr, std::memory_order order, T val1) {
          return op(SET, gptr, order, val1).then([](T val1){});
        }
        future<T> get(global_ptr<T> gptr, std::memory_order order) {
          return op(GET, gptr, order);
        }
        future<> inc(global_ptr<T> gptr, std::memory_order order) {
          return op(INC, gptr, order).then([](T val1){});
        }
        future<> dec(global_ptr<T> gptr, std::memory_order order) {
          return op(DEC, gptr, order).then([](T val1){});
        }
        future<T> finc(global_ptr<T> gptr, std::memory_order order) {
          return op(FINC, gptr, order);
        }
        future<T> fdec(global_ptr<T> gptr, std::memory_order order) {
          return op(FDEC, gptr, order);
        }
        future<> add(global_ptr<T> gptr, std::memory_order order, T val1) {
          return op(ADD, gptr, order, val1).then([](T val1){});
        }
        future<> sub(global_ptr<T> gptr, std::memory_order order, T val1) {
          return op(SUB, gptr, order, val1).then([](T val1){});
        }
        future<T> fadd(global_ptr<T> gptr, std::memory_order order, T val1) {
          return op(FADD, gptr, order, val1);
        }
        future<T> fsub(global_ptr<T> gptr, std::memory_order order, T val1) {
          return op(FSUB, gptr, order, val1);
        }
        future<T> cswap(global_ptr<T> gptr, std::memory_order order, T val1, T val2) {
          return op(CSWAP, gptr, order, val1, val2);
        }
    };
  } // namespace atomic
} // namespace upcxx

#endif
