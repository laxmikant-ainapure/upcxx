#include <gasnet_ratomic.h>
#include <upcxx/diagnostic.hpp>
#include <upcxx/atomic.hpp>
#include <upcxx/backend/gasnet/runtime_internal.hpp>

namespace gasnet = upcxx::backend::gasnet;
namespace backend = upcxx::backend;
namespace atomic = upcxx::atomic;

//enum AOP : char { GET, SET, ADD, FADD, SUB, FSUB, INC, FINC, DEC, FDEC, CSWAP };
static std::string atomic_op_str[] = { 
  "GET", "SET", "ADD", "FADD", "SUB", "FSUB", "INC", "FINC", "DEC", "FDEC", "CSWAP" };
static gex_OP_t to_gex_op[] = { 
  GEX_OP_GET, GEX_OP_SET, GEX_OP_ADD, GEX_OP_FADD, GEX_OP_SUB, GEX_OP_FSUB, 
  GEX_OP_INC, GEX_OP_FINC, GEX_OP_DEC, GEX_OP_FDEC, GEX_OP_CSWAP };

// Specializers for conversion of standard integer types to gasnet types.
template<typename T> gex_DT_t gex_dt();
template<> gex_DT_t gex_dt<int32_t>() { return GEX_DT_I32; }
template<> gex_DT_t gex_dt<int64_t>() { return GEX_DT_I64; }
template<> gex_DT_t gex_dt<uint32_t>() { return GEX_DT_U32; }
template<> gex_DT_t gex_dt<uint64_t>() { return GEX_DT_U64; }

// wrapper around gasnet function
template<typename T>
uintptr_t gex_AD_OpNB(gex_AD_t, T*, upcxx::global_ptr<T>, gex_OP_t, T, T, gex_Flags_t);
// Specializers that wrap the gasnet integer type operations.
#define SET_GEX_OP(T, GT) \
  template<> \
  uintptr_t gex_AD_OpNB<T>(gex_AD_t ad, T *p, upcxx::global_ptr<T> gp, gex_OP_t opcode, \
                           T val1, T val2, gex_Flags_t flags) { \
    return reinterpret_cast<uintptr_t>( \
        gex_AD_OpNB_##GT(ad, p, gp.rank_, gp.raw_ptr_, opcode, val1, val2, flags));}

SET_GEX_OP(int32_t, I32);
SET_GEX_OP(int64_t, I64);
SET_GEX_OP(uint32_t, U32);
SET_GEX_OP(uint64_t, U64);

template<typename T>
atomic::domain<T>::domain(std::vector<int> ops, int flags) {
  gex_ops = 0;
  for (auto next_op : ops) gex_ops |= to_gex_op[next_op];
  // Create the gasnet atomic domain for the world team.
  // QUERY: do we ever need to set any of the flags?
  gex_AD_Create(reinterpret_cast<gex_AD_t*>(&gex_ad), gasnet::world_team, gex_dt<T>(), gex_ops, flags);
}

template<typename T>
atomic::domain<T>::~domain() {
  // Destroy the gasnet atomic domain
  gex_AD_Destroy(reinterpret_cast<gex_AD_t>(gex_ad));
}

template<typename T> 
upcxx::future<T> atomic::domain<T>::op(upcxx::atomic::AOP aop, upcxx::global_ptr<T> gptr, 
                                       std::memory_order order, T val1, T val2) {
  gex_OP_t gex_op = to_gex_op[aop];
  // Fail if attempting to use an atomic operation not part of this domain.
  UPCXX_ASSERT(gex_op & gex_ops, 
               "Atomic operation " << atomic_op_str[aop] << " not included in domain\n");
  // The class that handles the gasnet event.
  // Must be declared final for the 'delete this' call.
  struct op_cb final: gasnet::handle_cb {
    // The promise to fulfill when the operation completes.
    promise<T> p;
    // The result of the operation - may be ignored.
    T result;
    // The callback executed upon event completion.
    void execute_and_delete(gasnet::handle_cb_successor) {
      // Now we are running in internal progress - can't fulfill until user progress.
      backend::during_user(
              [this]() {
                // The operation has completed - fulfill the result and delete callback object.
                p.fulfill_result(result);
                delete this;
              });
    }
  };
  // Create the callback object..
  auto *cb = new op_cb();
  // select the appropriate flags for the memory order
  gex_Flags_t flags = 0;
  if (order == std::memory_order_acquire) flags |= GEX_FLAG_AD_ACQ;
  else if (order == std::memory_order_release) flags |= GEX_FLAG_AD_REL;
  else if (order == std::memory_order_acq_rel) flags |= (GEX_FLAG_AD_ACQ | GEX_FLAG_AD_REL);
  // Get the handle for the gasnet function.
  // FIXME: flags will depend on the memory order
  cb->handle = gex_AD_OpNB<T>(reinterpret_cast<gex_AD_t>(gex_ad), &cb->result, gptr, gex_op, 
                              val1, val2, flags);
  // Get the future from the callback object.
  auto ans = cb->p.get_future();
  // Register the callback with gasnet.
  gasnet::register_cb(cb);
  // Make sure UPCXX does internal work after the gasnet call.
  gasnet::after_gasnet();
  // Return the future of the callback object.
  return ans;
}

// ensure classes are defined for these types
template class atomic::domain<int32_t>;
template class atomic::domain<int64_t>;
template class atomic::domain<uint32_t>;
template class atomic::domain<uint64_t>;


