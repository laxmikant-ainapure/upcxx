#ifndef _4fd2caba_e406_4f0e_ab34_0e0224ec36a5
#define _4fd2caba_e406_4f0e_ab34_0e0224ec36a5

/**
 * atomic.hpp
 */
#include <gasnet_ratomic.h>
#include <upcxx/backend/gasnet/runtime_internal.hpp>
#include <upcxx/backend.hpp>
#include <upcxx/global_ptr.hpp>
#include <upcxx/rpc.hpp>


namespace upcxx {
  namespace atomic {
    namespace detail {
      gex_AD_t implicit_domain_int32_t;
      gex_AD_t implicit_domain_int64_t;
      
      template <typename T>
      uintptr_t generic_gex_AD_OpNB(T *rp, gex_Rank_t trank, void *taddr, 
                                    gex_OP_t opcode, T op1, T op2, gex_Flags_t flags);
      
      template <>
      uintptr_t generic_gex_AD_OpNB<int32_t>(int32_t *rp, gex_Rank_t trank, void *taddr, 
                                             gex_OP_t opcode, int32_t op1, int32_t op2, 
                                             gex_Flags_t flags)
      {
        return reinterpret_cast<uintptr_t>(gex_AD_OpNB_I32(implicit_domain_int32_t, rp, 
            trank, taddr, opcode, op1, op2, flags));
      }
      
      template <>
      uintptr_t generic_gex_AD_OpNB<int64_t>(int64_t *rp, gex_Rank_t trank, void *taddr, 
                                             gex_OP_t opcode, int64_t op1, int64_t op2, 
                                             gex_Flags_t flags)
      {
        return reinterpret_cast<uintptr_t>(gex_AD_OpNB_I64(implicit_domain_int64_t, rp, 
            trank, taddr, opcode, op1, op2, flags));
      }
    }
    
    void init_implicit_domains(void) {
      gex_OP_t gex_ops = GEX_OP_SET|GEX_OP_GET|GEX_OP_ADD|GEX_OP_SUB|GEX_OP_FADD|GEX_OP_FSUB|
          GEX_OP_INC|GEX_OP_FINC|GEX_OP_DEC|GEX_OP_FDEC|GEX_OP_CSWAP;
      gex_AD_Create(&detail::implicit_domain_int32_t, backend::gasnet::world_team, 
          GEX_DT_I32, gex_ops, 0);
      gex_AD_Create(&detail::implicit_domain_int64_t, backend::gasnet::world_team, 
          GEX_DT_I64, gex_ops, 0);
    }
    
    template <typename T>
    future<T> get(global_ptr<T> p, std::memory_order order)	{
      return rpc(p.where(), [](global_ptr<T> p) { return *p.local(); }, p);
    }
    
    template <typename T>
    future<> set(global_ptr<T> p, T val, std::memory_order order)	{
      return rpc(p.where(), [](global_ptr<T> p, T val) { *(p.local()) = val; }, p, val);
    }
    
    template <typename T>
    future<T> fetch_add(global_ptr<T> gptr, T val, std::memory_order order) {
      
      struct my_cb final: backend::gasnet::handle_cb {
        promise<T> p;
        T result;
        
        void execute_and_delete(backend::gasnet::handle_cb_successor) {
          // we are now running in "internal" progress, cant fulfill until
          // user progress
          if (false) {
            upcxx::backend::during_user(std::move(p), result);
            delete this;
          } else {
            backend::during_user(
                [this]() {
                  p.fulfill_result(result);
                  delete this; // need "final" for this delete
                });
          }
        }
      };
      
      my_cb *cb = new my_cb;
      
      cb->handle = detail::generic_gex_AD_OpNB<T>(&cb->result, gptr.rank_, gptr.raw_ptr_, 
          GEX_OP_FADD, val, 0, 0);
      
      auto ans = cb->p.get_future();
      backend::gasnet::register_cb(cb);
      backend::gasnet::after_gasnet();
      
      return ans;
    }

  }
} // namespace upcxx

#endif
