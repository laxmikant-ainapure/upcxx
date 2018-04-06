#include <upcxx/diagnostic.hpp>
#include <upcxx/atomic.hpp>
#include <upcxx/backend/gasnet/runtime_internal.hpp>

#if UPCXX_BACKEND_GASNET && !NOBS_DISCOVERY
  #include <gasnet_ratomic.h>
#endif

#include <sstream>
#include <string>

namespace gasnet = upcxx::backend::gasnet;

using upcxx::atomic_domain;
using upcxx::atomic_op;
using upcxx::intrank_t;
using upcxx::global_ptr;

using std::int32_t;
using std::uint32_t;
using std::int64_t;
using std::uint64_t;
using std::uintptr_t;

static_assert(
  sizeof(gex_AD_t) == sizeof(uintptr_t), 
  "Mismatch between underying gasnet handle size and UPC++ implementation"
);
  
namespace {
  // translate upc++ atomic operations into gex ids
  // load, store, add, fetch_add, sub, fetch_sub, inc, fetch_inc, dec, fetch_dec, compare_exchange
  constexpr int to_gex_op_map[] = { 
    GEX_OP_GET, GEX_OP_SET, GEX_OP_ADD, GEX_OP_FADD, GEX_OP_SUB, GEX_OP_FSUB, 
    GEX_OP_INC, GEX_OP_FINC, GEX_OP_DEC, GEX_OP_FDEC, GEX_OP_CSWAP };

  // for error messages
  constexpr const char *atomic_op_str[] = {
    "load", "store", "add", "fetch_add", "sub", "fetch_sub", "inc", "fetch_inc", 
    "dec", "fetch_dec", "compare_exchange" };

  constexpr int op_count = int(sizeof(atomic_op_str)/sizeof(atomic_op_str[0]));
  
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

  std::string opset_gex_to_string(int opset) {
    std::stringstream ss;
    int n = 0;
    ss << '{';
    if(opset == 0)
      ss << "<empty!>";
    else {
      for(int op=0; op < op_count; op++) {
        if(opset & to_gex_op_map[op]) {
          if(n++) ss << ',';
          ss << atomic_op_str[op];
        }
      }
    }
    ss << '}';
    return ss.str();
  }

  template<typename T>
  constexpr gex_DT_t get_gex_dt();
  template<>
  constexpr gex_DT_t get_gex_dt<int32_t>() { return GEX_DT_I32; }
  template<>
  constexpr gex_DT_t get_gex_dt<uint32_t>() { return GEX_DT_U32; }
  template<>
  constexpr gex_DT_t get_gex_dt<int64_t>() { return GEX_DT_I64; }
  template<>
  constexpr gex_DT_t get_gex_dt<uint64_t>() { return GEX_DT_U64; }
  
  template<typename T>
  gex_Event_t shim_gex_AD_OpNB(
      gex_AD_t ad, T *p, intrank_t rank, void *addr,
      int op, T val1, T val2, int flags
    );

  template<>
  gex_Event_t shim_gex_AD_OpNB<int32_t>(
      gex_AD_t ad, int32_t *p, intrank_t rank, void *addr,
      int op, int32_t val1, int32_t val2, int flags
    ) {
    return gex_AD_OpNB_I32(ad, p, rank, addr, op, val1, val2, flags);
  }
  template<>
  gex_Event_t shim_gex_AD_OpNB<uint32_t>(
      gex_AD_t ad, uint32_t *p, intrank_t rank, void *addr,
      int op, uint32_t val1, uint32_t val2, int flags
    ) {
    return gex_AD_OpNB_U32(ad, p, rank, addr, op, val1, val2, flags);
  }
  template<>
  gex_Event_t shim_gex_AD_OpNB<int64_t>(
      gex_AD_t ad, int64_t *p, intrank_t rank, void *addr,
      int op, int64_t val1, int64_t val2, int flags
    ) {
    return gex_AD_OpNB_I64(ad, p, rank, addr, op, val1, val2, flags);
  }
  template<>
  gex_Event_t shim_gex_AD_OpNB<uint64_t>(
      gex_AD_t ad, uint64_t *p, intrank_t rank, void *addr,
      int op, uint64_t val1, uint64_t val2, int flags
    ) {
    return gex_AD_OpNB_U64(ad, p, rank, addr, op, val1, val2, flags);
  }
}

template<typename T>
atomic_domain<T>::atomic_domain(std::vector<atomic_op> const &ops, int flags) {
  atomic_gex_ops = 0;
  for (auto next_op : ops) atomic_gex_ops |= to_gex_op_map[static_cast<int>(next_op)];

  if(atomic_gex_ops != 0) {
    // Create the gasnet atomic domain for the world team.
    gex_AD_Create(reinterpret_cast<gex_AD_t*>(&ad_gex_handle), gasnet::world_team, 
                  get_gex_dt<T>(), atomic_gex_ops, flags);
  }
  else
    ad_gex_handle = 1;
}

template<typename T>
atomic_domain<T>::~atomic_domain() {
  // Destroy the gasnet atomic domain
  if (atomic_gex_ops) gex_AD_Destroy(reinterpret_cast<gex_AD_t>(ad_gex_handle));
}

template<typename T>
void atomic_domain<T>::call_gex_AD_OpNB(
    T *p, global_ptr<T> gp, atomic_op opcode,
    T val1, T val2,
    std::memory_order order,
    gasnet::handle_cb *cb
  ) {

  int op_gex = to_gex_op_map[static_cast<int>(opcode)];

  UPCXX_ASSERT(op_gex & atomic_gex_ops,
    "Atomic operation '" << atomic_op_str[static_cast<int>(opcode)] << "'"
    " not in domain's operation set '" << opset_gex_to_string(atomic_gex_ops) << "'\n");

  int flags = get_gex_flags(order);

  gex_Event_t h = shim_gex_AD_OpNB<T>(
    reinterpret_cast<gex_AD_t>(ad_gex_handle), p,
    gp.rank_, gp.raw_ptr_, op_gex, val1, val2, flags
  );

  cb->handle = reinterpret_cast<uintptr_t>(h);

  gasnet::register_cb(cb);
  gasnet::after_gasnet();
}

template class upcxx::atomic_domain<int32_t>;
template class upcxx::atomic_domain<uint32_t>;
template class upcxx::atomic_domain<int64_t>;
template class upcxx::atomic_domain<uint64_t>;


