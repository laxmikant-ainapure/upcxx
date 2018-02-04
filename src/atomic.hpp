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

#ifdef __GNUG__
#include <memory>
#include <cxxabi.h>
#endif

namespace upcxx {
  namespace atomic {

    enum operation {
      GET, SET,
      ADD, FADD,
      SUB, FSUB,
      INC, FINC,
      DEC, FDEC,
      CSWAP
    };

    namespace detail {

      gex_OP_t get_gex_ops(std::vector<operation> ops)
      {
        gex_OP_t gex_op = 0;

        for (auto op : ops) {
          switch (op) {
            case SET: gex_op |= GEX_OP_SET; break;
            case GET: gex_op |= GEX_OP_GET; break;
            case ADD: gex_op |= GEX_OP_ADD; break;
            case FADD: gex_op |= GEX_OP_FADD; break;
            case SUB: gex_op |= GEX_OP_SUB; break;
            case FSUB: gex_op |= GEX_OP_FSUB; break;
            case INC: gex_op |= GEX_OP_INC; break;
            case FINC: gex_op |= GEX_OP_FINC; break;
            case DEC: gex_op |= GEX_OP_DEC; break;
            case FDEC: gex_op |= GEX_OP_FDEC; break;
            case CSWAP: gex_op |= GEX_OP_CSWAP; break;
              break;
          }
        }
        return gex_op;
      }
    }

    template<typename T>
    struct domain {};

    template<>
    struct domain<int64_t> {
      gex_AD_t gex_ad;

      domain(std::vector<operation> ops, int flags = 0) {
        gex_AD_Create(&gex_ad, backend::gasnet::world_team, GEX_DT_I64, detail::get_gex_ops(ops),
            flags);
      }

      ~domain() {
        gex_AD_Destroy(gex_ad);
      }
    };


    namespace detail {

      template<typename T>
      uintptr_t generic_gex_AD_OpNB(gex_AD_t ad, T *rp, gex_Rank_t trank, void *taddr,
                                    gex_OP_t opcode, T op1, T op2, gex_Flags_t flags);

#define ADD_OP_DOMAIN(TYPE, GEXTYPE) \
      template<> \
      uintptr_t generic_gex_AD_OpNB<TYPE>(gex_AD_t ad, TYPE *rp, gex_Rank_t trank, void *taddr, \
                                          gex_OP_t opcode, TYPE op1, TYPE op2, gex_Flags_t flags) \
      { return reinterpret_cast<uintptr_t>(gex_AD_OpNB_##GEXTYPE(ad, rp, trank, taddr, \
      opcode, op1, op2, flags)); }

      ADD_OP_DOMAIN(int32_t, I32);
      ADD_OP_DOMAIN(int64_t, I64);
      ADD_OP_DOMAIN(uint32_t, U32);
      ADD_OP_DOMAIN(uint64_t, U64);

    }


    template<typename T>
    future<T> get(global_ptr<T> gptr, std::memory_order order, const domain<T> &d) {

      struct my_cb final: backend::gasnet::handle_cb {
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

      my_cb *cb = new my_cb;
      cb->handle = detail::generic_gex_AD_OpNB<T>(d.gex_ad, &cb->result,
          gptr.rank_, gptr.raw_ptr_, GEX_OP_GET, 0, 0, 0);
      auto ans = cb->p.get_future();
      backend::gasnet::register_cb(cb);
      backend::gasnet::after_gasnet();
      return ans;
    }


    template<typename T>
    future<> set(global_ptr<T> gptr, T val, std::memory_order order, const domain<T> &d)	{

      struct my_cb final: backend::gasnet::handle_cb {
        promise<> p;

        void execute_and_delete(backend::gasnet::handle_cb_successor) {
          if (false) {
            upcxx::backend::during_user(std::move(p));
            delete this;
          } else {
            backend::during_user(
                [this]() {
                  p.fulfill_result();
                  delete this;
                });
          }
        }
      };

      my_cb *cb = new my_cb;
      cb->handle = detail::generic_gex_AD_OpNB<T>(d.gex_ad, nullptr,
          gptr.rank_, gptr.raw_ptr_, GEX_OP_SET, val, 0, 0);
      auto ans = cb->p.get_future();
      backend::gasnet::register_cb(cb);
      backend::gasnet::after_gasnet();
      return ans;
    }


    template<typename T>
    future<T> fadd(global_ptr<T> gptr, T val, std::memory_order order, const domain<T> &d) {

      struct my_cb final: backend::gasnet::handle_cb {
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

      my_cb *cb = new my_cb;
      cb->handle = detail::generic_gex_AD_OpNB<T>(d.gex_ad, &cb->result,
          gptr.rank_, gptr.raw_ptr_, GEX_OP_FADD, val, 0, 0);
      auto ans = cb->p.get_future();
      backend::gasnet::register_cb(cb);
      backend::gasnet::after_gasnet();
      return ans;
    }


    template<typename T>
    future<T> fsub(global_ptr<T> gptr, T val, std::memory_order order, const domain<T> &d) {

      struct my_cb final: backend::gasnet::handle_cb {
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

      my_cb *cb = new my_cb;
      cb->handle = detail::generic_gex_AD_OpNB<T>(d.gex_ad, &cb->result,
          gptr.rank_, gptr.raw_ptr_, GEX_OP_FSUB, val, 0, 0);
      auto ans = cb->p.get_future();
      backend::gasnet::register_cb(cb);
      backend::gasnet::after_gasnet();
      return ans;
    }

  }
} // namespace upcxx

#endif
