#ifndef _4fd2caba_e406_4f0e_ab34_0e0224ec36a5
#define _4fd2caba_e406_4f0e_ab34_0e0224ec36a5

#include <upcxx/backend.hpp>
#include <upcxx/completion.hpp>
#include <upcxx/global_ptr.hpp>

#include <upcxx/backend/gasnet/runtime.hpp>

#include <climits>
#include <cstdint>
#include <vector>
#include <type_traits>

namespace upcxx {
  // All supported atomic operations.
  enum class atomic_op : int { load, store, compare_exchange,
                               add, fetch_add,
                               sub, fetch_sub,
                               mul, fetch_mul,
                               min, fetch_min,
                               max, fetch_max,
                               bit_and, fetch_bit_and,
                               bit_or, fetch_bit_or,
                               bit_xor, fetch_bit_xor,
                               inc, fetch_inc,
                               dec, fetch_dec };
  
  namespace detail {
    enum class amo_done : int { none, operation };
  }
  
  // Atomic domain for an ?int*_t type.
  template<typename T>
  class atomic_domain {
    private:
      // for checking type is 32 or 64-bit non-const integral type
      static constexpr bool is_atomic =
        std::is_integral<T>::value && !std::is_const<T>::value &&
        (sizeof(T) * CHAR_BIT == 32 || sizeof(T) * CHAR_BIT == 64);
      
      static_assert(is_atomic,
          "Atomic domains only supported on non-const 32 and 64-bit integral types");

      // Our encoding is that if both fields are zero than this is a
      // non-constructed object. Otherwise, if atomic_gex_ops is zero then
      // this is a constructed but empty domain which was not registered with
      // gasnet (hence ad_gex_handle was not produced by gasnet). Otherwise,
      // this domain was built by gasnet.

      // The or'd values for the atomic operations.
      int atomic_gex_ops = 0;
      // The opaque gasnet atomic domain handle.
      std::uintptr_t ad_gex_handle = 0;
      team *parent_tm_;
      
      // call to backend gasnet function
      detail::amo_done inject(
        T*, upcxx::global_ptr<T>, atomic_op, T, T,
        std::memory_order order,
        backend::gasnet::handle_cb*
      );
      
      // event values for non-fetching operations
      struct nofetch_aop_event_values {
        template<typename Event>
        using tuple_t = std::tuple<>;
      };
      // event values for fetching operations
      struct fetch_aop_event_values {
        template<typename Event>
        using tuple_t = typename std::conditional<
            std::is_same<Event, operation_cx_event>::value, std::tuple<T>, std::tuple<> >::type;
      };

      // The class that handles the gasnet event. This is for non-fetching ops.
      // Must be declared final for the 'delete this' call.
      template<typename CxStateHere>
      struct nofetch_op_cb final: backend::gasnet::handle_cb {
        CxStateHere state_here;

        nofetch_op_cb(CxStateHere state_here) : state_here{std::move(state_here)} {}

        // The callback executed upon event completion.
        void execute_and_delete(backend::gasnet::handle_cb_successor) override {
          this->state_here.template operator()<operation_cx_event>();
          delete this;
        }
      };

      // The class that handles the gasnet event. For fetching ops.
      template<typename CxStateHere>
      struct fetch_op_cb final: backend::gasnet::handle_cb {
        CxStateHere state_here;
        T result;

        fetch_op_cb(CxStateHere state_here) : state_here{std::move(state_here)} {}

        void execute_and_delete(backend::gasnet::handle_cb_successor) override {
          this->state_here.template operator()<operation_cx_event>(std::move(result));
          delete this;
        }
      };

      // convenience declarations
      template<typename Cxs>
      using FETCH_RTYPE = typename detail::completions_returner<detail::event_is_here,
          fetch_aop_event_values, Cxs>::return_t;
      template<typename Cxs>
      using NOFETCH_RTYPE = typename detail::completions_returner<detail::event_is_here,
          nofetch_aop_event_values, Cxs>::return_t;
      using FUTURE_CX = completions<future_cx<operation_cx_event> >;

      // generic fetching atomic operation
      template<typename Cxs = FUTURE_CX>
      FETCH_RTYPE<Cxs> fop(atomic_op aop, global_ptr<T> gptr, std::memory_order order,
                           T val1 = 0, T val2 = 0, Cxs cxs = Cxs{{}}) {
        UPCXX_ASSERT(atomic_gex_ops != 0 || ad_gex_handle != 0, "Atomic domain is not constructed");
        UPCXX_ASSERT((detail::completions_has_event<Cxs, operation_cx_event>::value));
        UPCXX_ASSERT(gptr != nullptr, "Global pointer for atomic operation is null");
        
        // we only have local completion, not remote
        using cxs_here_t = detail::completions_state<detail::event_is_here,
            fetch_aop_event_values, Cxs>;
        
        // Create the callback object
        auto *cb = new fetch_op_cb<cxs_here_t>{cxs_here_t{std::move(cxs)}};
        
        auto returner = detail::completions_returner<detail::event_is_here,
            fetch_aop_event_values, Cxs>{cb->state_here};
        
        // execute the backend gasnet function
        detail::amo_done done = this->inject(
          &cb->result, gptr, aop, val1, val2, order, cb
        );
        
        switch(done) {
        case detail::amo_done::none:
          backend::gasnet::register_cb(cb);
          backend::gasnet::after_gasnet();
          break;
        case detail::amo_done::operation:
        default:
          backend::gasnet::get_handle_cb_queue().execute_outside(cb);
          break;
        }
        
        return returner();
      }

      // generic non-fetching atomic operation
      template<typename Cxs = FUTURE_CX>
      NOFETCH_RTYPE<Cxs> op(atomic_op aop, global_ptr<T> gptr, std::memory_order order,
                            T val1 = 0, T val2 = 0, Cxs cxs = Cxs{{}}) {
        UPCXX_ASSERT(atomic_gex_ops != 0 || ad_gex_handle != 0, "Atomic domain is not constructed");
        UPCXX_ASSERT((detail::completions_has_event<Cxs, operation_cx_event>::value));
        UPCXX_ASSERT(gptr != nullptr, "Global pointer for atomic operation is null");
        
        // we only have local completion, not remote
        using cxs_here_t = detail::completions_state<detail::event_is_here,
            nofetch_aop_event_values, Cxs>;
        
        // Create the callback object on stack..
        nofetch_op_cb<cxs_here_t> cb(cxs_here_t(std::move(cxs)));
        
        auto returner = detail::completions_returner<detail::event_is_here,
            nofetch_aop_event_values, Cxs>{cb.state_here};
        
        // execute the backend gasnet function
        detail::amo_done done = this->inject(
          nullptr, gptr, aop, val1, val2, order, &cb
        );
        
        switch(done) {
        case detail::amo_done::none:
          // move callback to heap since it lives asynchronously
          backend::gasnet::register_cb(new decltype(cb)(std::move(cb)));
          backend::gasnet::after_gasnet();
          break;
        case detail::amo_done::operation:
        default:
          // do callback's execute_and_delete, minus the delete
          cb.state_here.template operator()<operation_cx_event>();
          break;
        }
        
        return returner();
      }

    public:
      // default constructor doesn't do anything besides initializing both:
      //   atomic_gex_ops = 0, ad_gex_handle = 0
      atomic_domain() {}

      atomic_domain(atomic_domain &&that) {
        this->ad_gex_handle = that.ad_gex_handle;
        this->atomic_gex_ops = that.atomic_gex_ops;
        this->parent_tm_ = that.parent_tm_;
        // revert `that` to non-constructed state
        that.atomic_gex_ops = 0;
        that.ad_gex_handle = 0;
        that.parent_tm_ = nullptr;
      }

      // The constructor takes a vector of operations. Currently, flags is currently unsupported.
      atomic_domain(std::vector<atomic_op> const &ops, team &tm = upcxx::world());
      
      #if 0 // disabling move-assignment, for now
      atomic_domain &operator=(atomic_domain &&that) {
        // only allow assignment moves onto "dead" object
        UPCXX_ASSERT(atomic_gex_ops == 0,
                     "Move assignment is only allowed on a default-constructed atomic_domain");
        this->ad_gex_handle = that.ad_gex_handle;
        this->atomic_gex_ops = that.atomic_gex_ops;
        this->parent_tm_ = that.parent_tm_;
        // revert `that` to non-constructed state
        that.atomic_gex_ops = 0;
        that.ad_gex_handle = 0;
        that.parent_tm_ = nullptr;
        return *this;
      }
      #endif
      
      ~atomic_domain();

      void destroy(entry_barrier eb = entry_barrier::user);
      
      template<typename Cxs = FUTURE_CX>
      NOFETCH_RTYPE<Cxs> store(global_ptr<T> gptr, T val, std::memory_order order,
                               Cxs cxs = Cxs{{}}) {
        return op(atomic_op::store, gptr, order, val, (T)0, cxs);
      }
      template<typename Cxs = FUTURE_CX>
      FETCH_RTYPE<Cxs> load(global_ptr<T> gptr, std::memory_order order, Cxs cxs = Cxs{{}}) {
        return fop(atomic_op::load, gptr, order, (T)0, (T)0, cxs);
      }
      template<typename Cxs = FUTURE_CX>
      NOFETCH_RTYPE<Cxs> inc(global_ptr<T> gptr, std::memory_order order, Cxs cxs = Cxs{{}}) {
        return op(atomic_op::inc, gptr, order, (T)0, (T)0, cxs);
      }
      template<typename Cxs = FUTURE_CX>
      NOFETCH_RTYPE<Cxs> dec(global_ptr<T> gptr, std::memory_order order, Cxs cxs = Cxs{{}}) {
        return op(atomic_op::dec,gptr, order, (T)0, (T)0, cxs);
      }
      template<typename Cxs = FUTURE_CX>
      FETCH_RTYPE<Cxs> fetch_inc(global_ptr<T> gptr, std::memory_order order, Cxs cxs = Cxs{{}}) {
        return fop(atomic_op::fetch_inc, gptr, order, (T)0, (T)0, cxs);
      }
      template<typename Cxs = FUTURE_CX>
      FETCH_RTYPE<Cxs> fetch_dec(global_ptr<T> gptr, std::memory_order order, Cxs cxs = Cxs{{}}) {
        return fop(atomic_op::fetch_dec, gptr, order, (T)0, (T)0, cxs);
      }
      template<typename Cxs = FUTURE_CX>
      FETCH_RTYPE<Cxs> compare_exchange(global_ptr<T> gptr, T val1, T val2, std::memory_order order,
                                        Cxs cxs = Cxs{{}}) {
        return fop(atomic_op::compare_exchange, gptr, order, val1, val2, cxs);
      }
      
      #define UPCXX_AD_METHODS(name)\
        template<typename Cxs = FUTURE_CX>\
        FETCH_RTYPE<Cxs> fetch_##name(global_ptr<T> gptr, T val, std::memory_order order,\
                                      Cxs cxs = Cxs{{}}) {\
          return fop(atomic_op::fetch_##name, gptr, order, val, (T)0, cxs);\
        }\
        template<typename Cxs = FUTURE_CX>\
        NOFETCH_RTYPE<Cxs> name(global_ptr<T> gptr, T val, std::memory_order order,\
                                Cxs cxs = Cxs{{}}) {\
          return op(atomic_op::name, gptr, order, val, (T)0, cxs);\
        }
      UPCXX_AD_METHODS(add)
      UPCXX_AD_METHODS(sub)
      UPCXX_AD_METHODS(mul)
      UPCXX_AD_METHODS(min)
      UPCXX_AD_METHODS(max)
      UPCXX_AD_METHODS(bit_and)
      UPCXX_AD_METHODS(bit_or)
      UPCXX_AD_METHODS(bit_xor)
      #undef UPCXX_AD_METHODS
  };
} // namespace upcxx

#endif
