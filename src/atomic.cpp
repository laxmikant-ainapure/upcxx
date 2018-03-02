#include <gasnet_ratomic.h>
#include <upcxx/diagnostic.hpp>
#include <upcxx/atomic.hpp>
#include <upcxx/backend/gasnet/runtime_internal.hpp>

namespace gasnet = upcxx::backend::gasnet;
namespace backend = upcxx::backend;
namespace atomic = upcxx::atomic;

//enum AOP : char { GET, SET, ADD, FADD, SUB, FSUB, INC, FINC, DEC, FDEC, CSWAP };
static int to_gex_op_map[] = { 
  GEX_OP_GET, GEX_OP_SET, GEX_OP_ADD, GEX_OP_FADD, GEX_OP_SUB, GEX_OP_FSUB, 
  GEX_OP_INC, GEX_OP_FINC, GEX_OP_DEC, GEX_OP_FDEC, GEX_OP_CSWAP };

static int get_gex_flags(std::memory_order order) {
  int flags = 0;
  switch (order) {
    case std::memory_order_acquire: flags |= GEX_FLAG_AD_ACQ; break;
    case std::memory_order_release: flags |= GEX_FLAG_AD_REL; break;
    case std::memory_order_acq_rel: flags |= (GEX_FLAG_AD_ACQ | GEX_FLAG_AD_REL); break;
    case std::memory_order_relaxed: break;
    case std::memory_order_seq_cst:
      UPCXX_ASSERT(0, "Unsupported memory order: std::memory_order_seq_cst");
    case std::memory_order_consume:
      UPCXX_ASSERT(0, "Unsupported memory order: std::memory_order_consume");
      break;
  }
  return flags;
}

// Specializers for conversion of standard integer types to gasnet types.
template<typename T> gex_DT_t get_gex_dt();
template<> gex_DT_t get_gex_dt<int32_t>() { return GEX_DT_I32; }
template<> gex_DT_t get_gex_dt<int64_t>() { return GEX_DT_I64; }
template<> gex_DT_t get_gex_dt<uint32_t>() { return GEX_DT_U32; }
template<> gex_DT_t get_gex_dt<uint64_t>() { return GEX_DT_U64; }

static std::string atomic_op_str[] = {
  "GET", "SET", "ADD", "FADD", "SUB", "FSUB", "INC", "FINC", "DEC", "FDEC", "CSWAP" };


// wrapper around gasnet function
// Specializers that wrap the gasnet integer type operations.
#define SET_GEX_OP(T, GT) \
template<> \
void upcxx::detail::call_gex_AD_OpNB<T>(uintptr_t ad, T *p, \
        upcxx::global_ptr<T> gp, atomic::aop_type opcode, int allowed_ops, T val1, T val2, \
        std::memory_order order, gasnet::handle_cb *cb) { \
  int aop_gex = to_gex_op_map[opcode]; \
  UPCXX_ASSERT(aop_gex & allowed_ops, \
               "Atomic operation " << atomic_op_str[aop_gex] << " not included in domain\n"); \
  int flags = get_gex_flags(order); \
  gex_Event_t h = gex_AD_OpNB_##GT(reinterpret_cast<gex_AD_t>(ad), p, gp.rank_, gp.raw_ptr_, \
                                   aop_gex, val1, val2, flags); \
  cb->handle = reinterpret_cast<uintptr_t>(h); \
  gasnet::register_cb(cb); \
  gasnet::after_gasnet(); \
}

SET_GEX_OP(int32_t, I32);
SET_GEX_OP(int64_t, I64);
SET_GEX_OP(uint32_t, U32);
SET_GEX_OP(uint64_t, U64);

template<typename T>
atomic::domain<T>::domain(std::vector<int> ops, int flags) {
  aops_gex = 0;
  for (auto next_op : ops) aops_gex |= to_gex_op_map[next_op];
  // Create the gasnet atomic domain for the world team.
  // QUERY: do we ever need to set any of the flags?
  gex_AD_Create(reinterpret_cast<gex_AD_t*>(&ad_gex), gasnet::world_team, get_gex_dt<T>(), 
                aops_gex, flags);
}

template<typename T>
atomic::domain<T>::~domain() {
  // Destroy the gasnet atomic domain
  gex_AD_Destroy(reinterpret_cast<gex_AD_t>(ad_gex));
}

// ensure classes are defined for these types
template class atomic::domain<int32_t>;
template class atomic::domain<int64_t>;
template class atomic::domain<uint32_t>;
template class atomic::domain<uint64_t>;


