#ifndef _4fd2caba_e406_4f0e_ab34_0e0224ec36a5
#define _4fd2caba_e406_4f0e_ab34_0e0224ec36a5

#include <vector>
#include <upcxx/global_ptr.hpp>

namespace upcxx {
  namespace atomic {

    // All supported atomic operations.
    enum AOP : char { GET, SET, ADD, FADD, SUB, FSUB, INC, FINC, DEC, FDEC, CSWAP };

    // Atomic domain for an ?int*_t type.
    template<typename T>
    class domain {
      private:
        // The opaque gasnet atomic domain handle.
        uintptr_t gex_ad;
        // The or'd value for all the atomic operations.
        uint32_t gex_ops;
      public:
        // The constructor takes a vector of operations. Currently, flags is unsupported.
        domain(std::vector<AOP> ops, int flags = 0);
        ~domain();

      // Generic atomic operation. This can take 0, 1 or 2 operands.
      future<T> operation(AOP aop, global_ptr<T> gptr, T op1=0, T op2=0);

      // Convenience functions for all the operations.
      future<> set(global_ptr<T> gptr, T op1) { return operation(SET, gptr, op1).then([](T op1){}); }
      future<T> get(global_ptr<T> gptr) { return operation(GET, gptr); }
      future<> inc(global_ptr<T> gptr) { return operation(INC, gptr).then([](T op1){}); }
      future<> dec(global_ptr<T> gptr) { return operation(DEC, gptr).then([](T op1){}); }
      future<T> finc(global_ptr<T> gptr) { return operation(FINC, gptr); }
      future<T> fdec(global_ptr<T> gptr) { return operation(FDEC, gptr); }
      future<> add(global_ptr<T> gptr, T op1) { return operation(ADD, gptr, op1).then([](T op1){}); }
      future<> sub(global_ptr<T> gptr, T op1) { return operation(SUB, gptr, op1).then([](T op1){}); }
      future<T> fadd(global_ptr<T> gptr, T op1) { return operation(FADD, gptr, op1); }
      future<T> fsub(global_ptr<T> gptr, T op1) { return operation(FSUB, gptr, op1); }
      future<T> cswap(global_ptr<T> gptr, T op1, T op2) { return operation(CSWAP, gptr, op1, op2); }
    };

  } // namespace atomic
} // namespace upcxx

#endif
