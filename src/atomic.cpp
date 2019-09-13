#include <upcxx/diagnostic.hpp>
#include <upcxx/atomic.hpp>
#include <upcxx/backend/gasnet/runtime_internal.hpp>

#if UPCXX_BACKEND_GASNET && !NOBS_DISCOVERY
  #include <gasnet_ratomic.h>
#endif

#include <sstream>
#include <string>

namespace gasnet = upcxx::backend::gasnet;
namespace detail = upcxx::detail;

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
  constexpr int to_gex_op_map[] = { 
    GEX_OP_GET, GEX_OP_SET, GEX_OP_CSWAP,
    GEX_OP_ADD, GEX_OP_FADD,
    GEX_OP_SUB, GEX_OP_FSUB,
    GEX_OP_MULT, GEX_OP_FMULT,
    GEX_OP_MIN, GEX_OP_FMIN,
    GEX_OP_MAX, GEX_OP_FMAX,
    GEX_OP_AND, GEX_OP_FAND,
    GEX_OP_OR, GEX_OP_FOR,
    GEX_OP_XOR, GEX_OP_FXOR,
    GEX_OP_INC, GEX_OP_FINC,
    GEX_OP_DEC, GEX_OP_FDEC
  };
  
  // check a handful of enum mappings to ensure no insert/delete errors
  static_assert(to_gex_op_map[(int)upcxx::atomic_op::mul] == GEX_OP_MULT, "Uh-oh");
  static_assert(to_gex_op_map[(int)upcxx::atomic_op::fetch_max] == GEX_OP_FMAX, "Uh-oh");
  static_assert(to_gex_op_map[(int)upcxx::atomic_op::bit_and] == GEX_OP_AND, "Uh-oh");
  static_assert(to_gex_op_map[(int)upcxx::atomic_op::fetch_bit_xor] == GEX_OP_FXOR, "Uh-oh");
  static_assert(to_gex_op_map[(int)upcxx::atomic_op::compare_exchange] == GEX_OP_CSWAP, "Uh-oh");
  
  // for error messages
  constexpr const char *atomic_op_str[] = {
    "load", "store", "compare_exchange",
    "add", "fetch_add",
    "sub", "fetch_sub",
    "mul", "fetch_mul",
    "min", "fetch_min",
    "max", "fetch_max",
    "bit_and", "fetch_bit_and",
    "bit_or", "fetch_bit_or",
    "bit_xor", "fetch_bit_xor",
    "inc", "fetch_inc", 
    "dec", "fetch_dec"
  };

  constexpr int op_count = int(sizeof(atomic_op_str)/sizeof(atomic_op_str[0]));
  
  int get_gex_flags(std::memory_order order) {
    int flags = 0;
    switch (order) {
      case std::memory_order_acquire: flags = GEX_FLAG_AD_ACQ; break;
      case std::memory_order_release: flags = GEX_FLAG_AD_REL; break;
      case std::memory_order_acq_rel: flags = (GEX_FLAG_AD_ACQ | GEX_FLAG_AD_REL); break;
      case std::memory_order_relaxed: break;
      case std::memory_order_seq_cst:
        UPCXX_ASSERT(0, "Unsupported memory order: std::memory_order_seq_cst");
        break;
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
  template<>
  constexpr gex_DT_t get_gex_dt<float>() { return GEX_DT_FLT; }
  template<>
  constexpr gex_DT_t get_gex_dt<double>() { return GEX_DT_DBL; }
  
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
  template<>
  gex_Event_t shim_gex_AD_OpNB<float>(
      gex_AD_t ad, float *p, intrank_t rank, void *addr,
      int op, float val1, float val2, int flags
    ) {
    return gex_AD_OpNB_FLT(ad, p, rank, addr, op, val1, val2, flags);
  }
  template<>
  gex_Event_t shim_gex_AD_OpNB<double>(
      gex_AD_t ad, double *p, intrank_t rank, void *addr,
      int op, double val1, double val2, int flags
    ) {
    return gex_AD_OpNB_DBL(ad, p, rank, addr, op, val1, val2, flags);
  }
}

template<typename T>
atomic_domain<T>::atomic_domain(std::vector<atomic_op> const &ops, team &tm) {
  atomic_gex_ops = 0;
  for (auto next_op : ops) atomic_gex_ops |= to_gex_op_map[static_cast<int>(next_op)];
 
  if (std::is_floating_point<T>::value) {
    int prohibited_ops = atomic_gex_ops &
      (GEX_OP_AND|GEX_OP_FAND|GEX_OP_OR|GEX_OP_FOR|GEX_OP_XOR|GEX_OP_FXOR);
    UPCXX_ASSERT(prohibited_ops == 0,
      "atomic_domain on floating-point types may not use " << opset_gex_to_string(prohibited_ops) << std::endl);
  }

  parent_tm_ = &tm;
  
  if(atomic_gex_ops != 0) {
    // Create the gasnet atomic domain for the world team.
    gex_AD_Create(reinterpret_cast<gex_AD_t*>(&ad_gex_handle),
                  gasnet::handle_of(tm), 
                  get_gex_dt<T>(), atomic_gex_ops, /*flags=*/0);
  }
  else
    ad_gex_handle = 1;
}

template<typename T>
void atomic_domain<T>::destroy(entry_barrier eb) {
  UPCXX_ASSERT(backend::master.active_with_caller());
  
  backend::quiesce(*parent_tm_, eb);
  
  if(atomic_gex_ops) {
    gex_AD_Destroy(reinterpret_cast<gex_AD_t>(ad_gex_handle));
    atomic_gex_ops = 0;
  }
}

template<typename T>
atomic_domain<T>::~atomic_domain() {
  if(backend::init_count > 0) { // we don't assert on leaks after finalization
    UPCXX_ASSERT_ALWAYS(
      atomic_gex_ops == 0,
      "ERROR: `upcxx::atomic_domain::destroy()` must be called collectively before destructor."
    );
  }
}

template<typename T>
detail::amo_done atomic_domain<T>::inject(
    T *p, global_ptr<T> gp, atomic_op opcode,
    T val1, T val2,
    std::memory_order order,
    gasnet::handle_cb *cb
  ) {

  int op_gex = to_gex_op_map[static_cast<int>(opcode)];

  UPCXX_ASSERT(op_gex & atomic_gex_ops,
    "Atomic operation '" << atomic_op_str[static_cast<int>(opcode)] << "'"
    " not in domain's operation set '" << opset_gex_to_string(this->atomic_gex_ops) << "'\n");

  int flags = get_gex_flags(order);
  
  gex_Event_t h = shim_gex_AD_OpNB<T>(
    reinterpret_cast<gex_AD_t>(this->ad_gex_handle), p,
    gp.rank_, gp.raw_ptr_, op_gex, val1, val2,
    flags | GEX_FLAG_RANK_IS_JOBRANK
  );

  cb->handle = reinterpret_cast<uintptr_t>(h);
  
  return gex_Event_Test(h) == 0
    ? detail::amo_done::operation
    : detail::amo_done::none;
}

template class upcxx::atomic_domain<int32_t>;
template class upcxx::atomic_domain<uint32_t>;
template class upcxx::atomic_domain<int64_t>;
template class upcxx::atomic_domain<uint64_t>;
template class upcxx::atomic_domain<float>;
template class upcxx::atomic_domain<double>;

