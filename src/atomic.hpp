#ifndef _4fd2caba_e406_4f0e_ab34_0e0224ec36a5
#define _4fd2caba_e406_4f0e_ab34_0e0224ec36a5

#include <gasnet_ratomic.h>

#error "You can't include this header from headers included by the user"

#include <upcxx/backend/gasnet/runtime_internal.hpp>


namespace upcxx {
  namespace atomic {

    #error "Hello, Steve"
    /*
    Due to the header restriction, we cant use gasnet types since
    the gasnet headers arent available to us. You will need
    your own enum values and a mapping in the .cpp from those to
    gasnet aops. But, instead of an enum, I recommend is using an extern
    global variable per op, and using addresses of those as our "enum":

    // *** atomic.hpp ***
    namespace upcxx { namespace detail {

    extern const std::uintptr_t gex_op_get, gex_op_add, ...;

    }}
    
    // *** atomic.cpp ***
    #include <.../runtime_internal.hpp> // brings in gasnet

    namespace upcxx { namespace detail {

    const uintptr_t gex_op_get = GEX_OP_GET,
                    gex_op_add = GEX_OP_ADD,
                    ...;

    }}

    With this, you would replace occurrences of AOP's with
    "const std::uintptr_t*", or typedef AOP to uintptr_t since the user
    needs to use AOPs to construct a domain. Then at initiation time,
    just dereference the pointer to get gasnet's enum value.

    You also need to move initiation "gex_AD_OpNB_***" into the .cpp as
    well. I would wrap it in our own function, templated on the datatype,
    and passing in the op as "uintptr_t const*".

    // *** atomic.hpp ***
    // returns gasnet handle
    template<typename T>
    uintptr_t amo(..., uintptr_t const *op, T arg1, T arg2);

    // *** atomic.cpp ***
    // provide definition for each amo<T> individually. A macro is the
    // best way to factor out redundancies.
    template<>
    uintptr_t amo<int32_t>(..., uintptr_t const *op, int32_t arg1, int32_t arg2) {
      return reinterpret_cast<uintptr_t>(gex_AD_OpNB_I32(..., *op, arg1, arg2));
    }
    template<>
    uintptr_t amo<int64_t>(..., uintptr_t const *op, int64_t arg1, int64_t arg2) {
      return reinterpret_cast<uintptr_t>(gex_AD_OpNB_I64(..., *op, arg1, arg2));
    }
    // amo<uint32_t> and amo<uint64_t>
    */
      
           
    // All supported atomic operations.
    enum class AOP : gex_OP_t {
      GET = GEX_OP_GET, SET = GEX_OP_SET,
      ADD = GEX_OP_ADD, FADD = GEX_OP_FADD,
      SUB = GEX_OP_SUB, FSUB = GEX_OP_FSUB,
      INC = GEX_OP_INC, FINC = GEX_OP_FINC,
      DEC = GEX_OP_DEC, FDEC = GEX_OP_FDEC,
      CSWAP = GEX_OP_CSWAP
    };

    namespace detail {

      // Helper for error messages.
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

      #error "Hello, Steve"
      /* Again, you cant use gasnet types. My big comment explains how to
      pass the template type into the .cpp */
      // Specializers for conversion of standard integer types to gasnet types.
      template<typename T> gex_DT_t gex_dt();
      template<> gex_DT_t gex_dt<int32_t>() { return GEX_DT_I32; }
      template<> gex_DT_t gex_dt<int64_t>() { return GEX_DT_I64; }
      template<> gex_DT_t gex_dt<uint32_t>() { return GEX_DT_U32; }
      template<> gex_DT_t gex_dt<uint64_t>() { return GEX_DT_U64; }

      // Specializers that wrap the gasnet integer type operations.
      template<typename T>
      uintptr_t gex_op(gex_AD_t, T*, global_ptr<T>, gex_OP_t, T, T, gex_Flags_t);
      // FIXME: any way to avoid using macros?
#define SET_GEX_OP(T, GT) \
      template<> \
      inline uintptr_t gex_op<T>(gex_AD_t ad, T *p, global_ptr<T> gp, gex_OP_t opcode, \
                                 T op1, T op2, gex_Flags_t flags) { \
        return reinterpret_cast<uintptr_t>( \
            gex_AD_OpNB_##GT(ad, p, gp.rank_, gp.raw_ptr_, opcode, op1, op2, flags));}

      SET_GEX_OP(int32_t, I32);
      SET_GEX_OP(int64_t, I64);
      SET_GEX_OP(uint32_t, U32);
      SET_GEX_OP(uint64_t, U64);

    }

    // Atomic domain for an ?int*_t type.
    template<typename T>
    struct domain {
      #error "Hello, Steve"
      /* No gasnet, use uintptr_t. Anything that calls gasnet must be in
      .cpp */
      
      // The opaque gasnet atomic domain handle.
      gex_AD_t gex_ad;
      // The or'd value for all the atomic operations.
      gex_OP_t gex_ops;

      // The constructor takes a vector of operations. Currently, flags is unsupported.
      domain(std::vector<AOP> ops, int flags = 0) {
        gex_ops = 0;
        for (auto next_op : ops) gex_ops |= static_cast<gex_OP_t>(next_op);
        // Create the gasnet atomic domain for the world team.
        gex_AD_Create(&gex_ad, backend::gasnet::world_team, detail::gex_dt<T>(), gex_ops, flags);
      }

      ~domain() {
        // Destroy the gasnet atomic domain
        gex_AD_Destroy(gex_ad);
      }

      // Generic atomic operation. This can take 0, 1 or 2 operands.
      template<AOP OP>
      future<T> operation(global_ptr<T> gptr, T op1=0, T op2=0) {
        // Fail if attempting to use an atomic operation not part of this domain.
        UPCXX_ASSERT_ALWAYS((gex_OP_t)OP & gex_ops,
            "Atomic operation " << detail::get_aop_name(OP) << " not included in domain\n");
        // The class that handles the gasnet event.
        // Must be declared final for the 'delete this' call.
        struct op_cb final: backend::gasnet::handle_cb {
          // The promise to fulfill when the operation completes.
          promise<T> p;
          // The result of the operation - may be ignored.
          T result;
          // The callback executed upon event completion.
          void execute_and_delete(backend::gasnet::handle_cb_successor) {
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
        // Get the handle for the gasnet function.
        cb->handle = detail::gex_op<T>(gex_ad, &cb->result, gptr,
                                       static_cast<gex_OP_t>(OP), op1, op2, 0);
        // Get the future from the callback object.
        auto ans = cb->p.get_future();
        // Register the callback with gasnet.
        backend::gasnet::register_cb(cb);
        // Make sure UPCXX does internal work after the gasnet call.
        backend::gasnet::after_gasnet();
        // Return the future of the callback object.
        return ans;
      }

      // Convenience functions for all the operations.
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
