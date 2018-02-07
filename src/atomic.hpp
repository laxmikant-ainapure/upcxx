#ifndef _4fd2caba_e406_4f0e_ab34_0e0224ec36a5
#define _4fd2caba_e406_4f0e_ab34_0e0224ec36a5

#include <gasnet_ratomic.h>
#include <upcxx/backend/gasnet/runtime_internal.hpp>


namespace upcxx {
  namespace atomic {

    // all supported atomic operations
    enum class AOP : gex_OP_t {
      GET = GEX_OP_GET, SET = GEX_OP_SET,
      ADD = GEX_OP_ADD, FADD = GEX_OP_FADD,
      SUB = GEX_OP_SUB, FSUB = GEX_OP_FSUB,
      INC = GEX_OP_INC, FINC = GEX_OP_FINC,
      DEC = GEX_OP_DEC, FDEC = GEX_OP_FDEC,
      CSWAP = GEX_OP_CSWAP
    };

    namespace detail {
      std::string get_aop_name(AOP aop) {
        switch(aop) {
          case AOP::GET: return "GET";
          case AOP::SET: return "SET";
          case AOP::ADD: return "ADD";
          case AOP::FADD: return "FADD";
          case AOP::SUB: return "SUB";
          case AOP::FSUB: return "FSUB";
          default: return "Unknown";
        }
      }

      template<typename T> gex_DT_t gex_dt();
      template<> gex_DT_t gex_dt<int32_t>() { return GEX_DT_I32; }
      template<> gex_DT_t gex_dt<int64_t>() { return GEX_DT_I64; }
      template<> gex_DT_t gex_dt<uint32_t>() { return GEX_DT_U32; }
      template<> gex_DT_t gex_dt<uint64_t>() { return GEX_DT_U64; }

      template<typename T>
      uintptr_t gex_op(gex_AD_t, T*, upcxx::global_ptr<T>, gex_OP_t, T, T, gex_Flags_t);

      // FIXME: any way to avoid using macros?
#define SET_GEX_OP(T, GT) \
      template<> \
      inline uintptr_t gex_op<T>(gex_AD_t ad, T *p, upcxx::global_ptr<T> gp, gex_OP_t opcode, \
                                 T op1, T op2, gex_Flags_t flags) { \
        return reinterpret_cast<uintptr_t>( \
            gex_AD_OpNB_##GT(ad, p, gp.rank_, gp.raw_ptr_, opcode, op1, op2, flags));}

      SET_GEX_OP(int32_t, I32);
      SET_GEX_OP(int64_t, I64);
      SET_GEX_OP(uint32_t, U32);
      SET_GEX_OP(uint64_t, U64);

    }

    template<typename T>
    struct domain {
      gex_AD_t gex_ad;
      gex_OP_t gex_ops;

      domain(std::vector<AOP> ops, int flags = 0) {
        gex_ops = 0;
        for (auto next_op : ops) gex_ops |= static_cast<gex_OP_t>(next_op);
        gex_AD_Create(&gex_ad, backend::gasnet::world_team, detail::gex_dt<T>(), gex_ops, flags);
      }

      ~domain() {
        gex_AD_Destroy(gex_ad);
      }

      template<AOP OP>
      future<T> operation(global_ptr<T> gptr, T op1=0, T op2=0) {
        UPCXX_ASSERT_ALWAYS((gex_OP_t)OP & gex_ops,
            "Atomic operation " << detail::get_aop_name(OP) << " not included in domain\n");
        struct op_cb final: backend::gasnet::handle_cb {
          promise<T> p;
          T result;
          void execute_and_delete(backend::gasnet::handle_cb_successor) {
            if (false) {
              upcxx::backend::during_user(std::move(p), result);
              delete this;
            } else {
              backend::during_user(
                  [this]() {
                    p.fulfill_result(result);
                    delete this;
                  });
            }
          }
        };
        auto *cb = new op_cb();
        cb->handle = detail::gex_op<T>(gex_ad, &cb->result, gptr,
                                       static_cast<gex_OP_t>(OP), op1, op2, 0);
        auto ans = cb->p.get_future();
        backend::gasnet::register_cb(cb);
        backend::gasnet::after_gasnet();
        return ans;
      }

      future<> set(global_ptr<T> gptr, T op1) {
        return operation<AOP::SET>(gptr, op1).then([](T op1){}); }
      future<T> get(global_ptr<T> gptr) { return operation<AOP::GET>(gptr); }
      future<> inc(global_ptr<T> gptr) { return operation<AOP::INC>(gptr).then([](T op1){}); }
      future<> dec(global_ptr<T> gptr) { return operation<AOP::DEC>(gptr).then([](T op1){}); }
      future<T> finc(global_ptr<T> gptr) { return operation<AOP::FINC>(gptr); }
      future<T> fdec(global_ptr<T> gptr) { return operation<AOP::FDEC>(gptr); }
      future<> add(global_ptr<T> gptr, T op1) {
        return operation<AOP::ADD>(gptr, op1).then([](T op1){}); }
      future<> sub(global_ptr<T> gptr, T op1) {
        return operation<AOP::SUB>(gptr, op1).then([](T op1){}); }
      future<T> fadd(global_ptr<T> gptr, T op1) { return operation<AOP::FADD>(gptr, op1); }
      future<T> fsub(global_ptr<T> gptr, T op1) { return operation<AOP::FSUB>(gptr, op1); }
      future<T> cswap(global_ptr<T> gptr, T op1, T op2) { return operation<AOP::CSWAP>(gptr, op1, op2); }
    };

  } // namespace atomic
} // namespace upcxx

#endif
