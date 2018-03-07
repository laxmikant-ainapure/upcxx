#include <gasnet_ratomic.h>
#include <upcxx/diagnostic.hpp>
#include <upcxx/atomic.hpp>
#include <upcxx/backend/gasnet/runtime_internal.hpp>

namespace gasnet = upcxx::backend::gasnet;

namespace {
  // translate upc++ atomic operations into gex ids
  // load, store, add, fetch_add, sub, fetch_sub, inc, fetch_inc, dec, fetch_dec, compare_exchange
  int to_gex_op_map[] = { 
    GEX_OP_GET, GEX_OP_SET, GEX_OP_ADD, GEX_OP_FADD, GEX_OP_SUB, GEX_OP_FSUB, 
    GEX_OP_INC, GEX_OP_FINC, GEX_OP_DEC, GEX_OP_FDEC, GEX_OP_CSWAP };

  // for error messages
  std::string atomic_op_str[] = {
    "LOAD", "STORE", "ADD", "FETCH_ADD", "SUB", "FETCH_SUB", "INC", "FETCH_INC", 
    "DEC", "FETCH_DEC", "COCMPARE_EXCHANGE" };

  int get_gex_flags(std::memory_order order) {
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

  template<typename T>
  gex_DT_t get_gex_dt();
  template<>
  gex_DT_t get_gex_dt<int32_t>() { return GEX_DT_I32; }
  template<>
  gex_DT_t get_gex_dt<uint32_t>() { return GEX_DT_U32; }
  template<>
  gex_DT_t get_gex_dt<int64_t>() { return GEX_DT_I64; }
  template<>
  gex_DT_t get_gex_dt<uint64_t>() { return GEX_DT_U64; }
  template<>
  gex_DT_t get_gex_dt<float>() { return GEX_DT_FLT; }
  template<>
  gex_DT_t get_gex_dt<double>() { return GEX_DT_DBL; }
}


// wrapper around gasnet function
// Specializers that wrap the gasnet integer type operations.
#define SET_GEX_OP(T, GT) \
template<> \
void upcxx::atomic_domain<T>::call_gex_AD_OpNB(T *p, upcxx::global_ptr<T> gp, \
    upcxx::atomic_op opcode, T val1, T val2, std::memory_order order, gasnet::handle_cb *cb) { \
  int atomic_gex_op = to_gex_op_map[static_cast<int>(opcode)]; \
  UPCXX_ASSERT(atomic_gex_op & atomic_gex_ops, \
      "Atomic operation " << atomic_op_str[static_cast<int>(opcode)] << " not in domain\n"); \
  int flags = get_gex_flags(order); \
  gex_Event_t h = gex_AD_OpNB_##GT(reinterpret_cast<gex_AD_t>(ad_gex_handle), p, \
                                   gp.rank_, gp.raw_ptr_, atomic_gex_op, val1, val2, flags); \
  cb->handle = reinterpret_cast<uintptr_t>(h); \
  gasnet::register_cb(cb); \
  gasnet::after_gasnet(); \
}

SET_GEX_OP(int32_t, I32);
SET_GEX_OP(uint32_t, U32);
SET_GEX_OP(int64_t, I64);
SET_GEX_OP(uint64_t, U64);
SET_GEX_OP(float, FLT);
SET_GEX_OP(double, DBL);

template<typename T>
upcxx::atomic_domain<T>::atomic_domain(std::vector<atomic_op> const &ops, int flags) {
  UPCXX_ASSERT(!ops.empty(), "Need to specify at least one atomic_op for the atomic_domain");
  atomic_gex_ops = 0;
  for (auto next_op : ops) atomic_gex_ops |= to_gex_op_map[static_cast<int>(next_op)];
  // Create the gasnet atomic domain for the world team.
  gex_AD_Create(reinterpret_cast<gex_AD_t*>(&ad_gex_handle), gasnet::world_team, 
                get_gex_dt<T>(), atomic_gex_ops, flags);
}

template<typename T>
upcxx::atomic_domain<T>::~atomic_domain() {
  // Destroy the gasnet atomic domain
  if (ad_gex_handle) gex_AD_Destroy(reinterpret_cast<gex_AD_t>(ad_gex_handle));
}

template class upcxx::atomic_domain<int32_t>;
template class upcxx::atomic_domain<uint32_t>;
template class upcxx::atomic_domain<int64_t>;
template class upcxx::atomic_domain<uint64_t>;
template class upcxx::atomic_domain<float>;
template class upcxx::atomic_domain<double>;

