#ifndef _4fd2caba_e406_4f0e_ab34_0e0224ec36a5
#define _4fd2caba_e406_4f0e_ab34_0e0224ec36a5

#include <vector>
#include <upcxx/global_ptr.hpp>

namespace upcxx {
  namespace atomic {

    // All supported atomic operations.
    enum AOP: int { GET, SET, ADD, FADD, SUB, FSUB, INC, FINC, DEC, FDEC, CSWAP };

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
        domain(std::vector<int> ops, int flags = 0);
        ~domain();

        // Generic atomic operation. This can take 0, 1 or 2 operands.
        future<T> op(AOP aop, global_ptr<T> gptr, std::memory_order order, T val1=0, T val2=0);

        // Convenience functions for all the operations.
        future<> set(global_ptr<T> gptr, std::memory_order order, T val1) {
          return op(SET, gptr, order, val1).then([](T val1){});
        }
        future<T> get(global_ptr<T> gptr, std::memory_order order) {
          return op(GET, gptr, order);
        }
        future<> inc(global_ptr<T> gptr, std::memory_order order) {
          return op(INC, gptr, order).then([](T val1){});
        }
        future<> dec(global_ptr<T> gptr, std::memory_order order) {
          return op(DEC, gptr, order).then([](T val1){});
        }
        future<T> finc(global_ptr<T> gptr, std::memory_order order) {
          return op(FINC, gptr, order);
        }
        future<T> fdec(global_ptr<T> gptr, std::memory_order order) {
          return op(FDEC, gptr, order);
        }
        future<> add(global_ptr<T> gptr, std::memory_order order, T val1) {
          return op(ADD, gptr, order, val1).then([](T val1){});
        }
        future<> sub(global_ptr<T> gptr, std::memory_order order, T val1) {
          return op(SUB, gptr, order, val1).then([](T val1){});
        }
        future<T> fadd(global_ptr<T> gptr, std::memory_order order, T val1) {
          return op(FADD, gptr, order, val1);
        }
        future<T> fsub(global_ptr<T> gptr, std::memory_order order, T val1) {
          return op(FSUB, gptr, order, val1);
        }
        future<T> cswap(global_ptr<T> gptr, std::memory_order order, T val1, T val2) {
          return op(CSWAP, gptr, order, val1, val2);
        }
    };
  } // namespace atomic
} // namespace upcxx

#endif
