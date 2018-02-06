#ifndef _4fd2caba_e406_4f0e_ab34_0e0224ec36a5
#define _4fd2caba_e406_4f0e_ab34_0e0224ec36a5

/**
 * atomic.hpp
 *
 * Currently supports the following types:
 * int32_t
 * uint32_t
 * int64_t
 * uint64_t
 *
 */
#include <gasnet_ratomic.h>
#include <upcxx/backend/gasnet/runtime_internal.hpp>
#include <upcxx/backend.hpp>
#include <upcxx/global_ptr.hpp>
#include <upcxx/rpc.hpp>

namespace upcxx {
  namespace atomic {
    // all supported atomic operations
    enum operation {
      GET = GEX_OP_GET, SET = GEX_OP_SET,
      ADD = GEX_OP_ADD, FADD = GEX_OP_FADD,
      SUB = GEX_OP_SUB, FSUB = GEX_OP_FSUB,
      INC = GEX_OP_INC, FINC = GEX_OP_FINC,
      DEC = GEX_OP_DEC, FDEC = GEX_OP_FDEC,
      CSWAP = GEX_OP_CSWAP
    };

    template<typename T> gex_DT_t get_gex_dt();

    template<typename T>
    uintptr_t gex_op(gex_AD_t ad, T *p, upcxx::global_ptr<T> gp, gex_OP_t opcode, T op1, T op2,
                     gex_Flags_t flags);

#define SET_GEX_OP_DT(T, GT) \
    template<> gex_DT_t get_gex_dt<T>() { return GEX_DT_##GT; } \
    template<> \
    inline uintptr_t gex_op<T>(gex_AD_t ad, T *p, upcxx::global_ptr<T> gp, gex_OP_t opcode, \
                               T op1, T op2, gex_Flags_t flags) \
    { \
      return reinterpret_cast<uintptr_t>(gex_AD_OpNB_##GT(ad, p, gp.rank_, gp.raw_ptr_, opcode, \
          op1, op2, flags)); \
    }

    SET_GEX_OP_DT(int32_t, I32);
    SET_GEX_OP_DT(int64_t, I64);
    SET_GEX_OP_DT(uint32_t, U32);
    SET_GEX_OP_DT(uint64_t, U64);

    template<typename T>
    class domain {
      private:
        gex_AD_t gex_ad;

      public:
        domain(std::vector<operation> ops, int flags = 0) {
          gex_OP_t gex_ops = 0;
          for (auto op : ops) gex_ops |= op;
          gex_AD_Create(&gex_ad, backend::gasnet::world_team, get_gex_dt<T>(), gex_ops, flags);
        }

        ~domain() {
          gex_AD_Destroy(gex_ad);
        }

        future<T> op(global_ptr<T> gp, gex_OP_t opcode, T op1, T op2) const {

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
          cb->handle = gex_op<T>(gex_ad, &cb->result, gp, opcode, op1, op2, 0);
          auto ans = cb->p.get_future();
          backend::gasnet::register_cb(cb);
          backend::gasnet::after_gasnet();
          return ans;
        }
    };

    // GET, INC, FINC, DEC, FDEC
    template<operation OP, typename T>
    future<T> op(global_ptr<T> gptr, const domain<T> &d,
                 std::memory_order order = std::memory_order_relaxed)
    {
      return d.op(gptr, OP, 0, 0);
    }

    // FADD, FSUB, SET
    template<operation OP, typename T>
    future<T> op(global_ptr<T> gptr, T val, const domain<T> &d,
                 std::memory_order order = std::memory_order_relaxed)
    {
      return d.op(gptr, OP, val, 0);
    }

    // CSWAP
    template<operation OP, typename T>
    future<T> op(global_ptr<T> gptr, T val1, T val2, const domain<T> &d,
                 std::memory_order order = std::memory_order_relaxed)
    {
      return d.op(gptr, OP, val1, val2);
    }

    template<typename T>
    using op_ptr = future<T> (*)(global_ptr<T> gptr, T val, const domain<T> &d, std::memory_order);

    // NB: this alias generates a warning for C++11, but still seems to work
    template<typename T> constexpr op_ptr<T> fadd = op<FADD, T>;

  } // namespace atomic
} // namespace upcxx

#endif
