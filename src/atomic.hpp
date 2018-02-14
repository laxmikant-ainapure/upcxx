#ifndef _4fd2caba_e406_4f0e_ab34_0e0224ec36a5
#define _4fd2caba_e406_4f0e_ab34_0e0224ec36a5

#include <vector>
#include <upcxx/global_ptr.hpp>

namespace upcxx {
  namespace detail {
    template<typename T>
    uintptr_t gex_AD_OpNB(uintptr_t, T*, upcxx::global_ptr<T>, int, T, T, int);
    template<>
    uintptr_t gex_AD_OpNB(uintptr_t ad, int64_t* p, upcxx::global_ptr<int64_t>,
                          int opcode, int64_t val1, int64_t val2, int flags);
    int get_gex_flags(std::memory_order order);
    int to_gex_op(int opcode);
  }

  namespace atomic {
    // All supported atomic operations.
    enum AOP: int { GET, SET, ADD, FADD, SUB, FSUB, INC, FINC, DEC, FDEC, CSWAP };
    static std::string atomic_op_str[] = {
      "GET", "SET", "ADD", "FADD", "SUB", "FSUB", "INC", "FINC", "DEC", "FDEC", "CSWAP" };

    // Atomic domain for an ?int*_t type.
    template<typename T>
    class domain {
      private:
        // The opaque gasnet atomic domain handle.
        uintptr_t gex_ad;
        // The or'd value for all the atomic operations.
        int gex_ops;
      public:
        // The constructor takes a vector of operations. Currently, flags is unsupported.
        domain(std::vector<int> ops, int flags = 0);
        ~domain();

        // Generic atomic operation. This can take 0, 1 or 2 operands.
        future<T> op(AOP aop, global_ptr<T> gptr, std::memory_order order, T val1=0, T val2=0) {
          int gex_op = upcxx::detail::to_gex_op(aop);
          // Fail if attempting to use an atomic operation not part of this domain.
          UPCXX_ASSERT(gex_op & gex_ops,
                       "Atomic operation " << atomic_op_str[aop] << " not included in domain\n");
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
          // select the appropriate flags for the memory order
          int flags = upcxx::detail::get_gex_flags(order);
          // Get the handle for the gasnet function.
          cb->handle = upcxx::detail::gex_AD_OpNB<T>(gex_ad, &cb->result, gptr, gex_op,
              val1, val2, flags);
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
