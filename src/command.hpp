#ifndef _3589ecdd_36fa_4240_ade2_c807b184ac7f
#define _3589ecdd_36fa_4240_ade2_c807b184ac7f

#include <upcxx/diagnostic.hpp>
#include <upcxx/future.hpp>
#include <upcxx/global_fnptr.hpp>
#include <upcxx/packing.hpp>

// Commands are callable objects that have been packed into a parcel.

namespace upcxx {
  // command<Arg...>: Collection of static functions for managing commands which
  // accept argument list of type Arg... when executed.
  template<typename ...Arg>
  class command;

  // command<>: Empty args-list specialization. 
  template<>
  class command<> {
    using executor_t = global_fnptr<void(parcel_reader&)>;

    template<typename Fn>
    static void the_executor(parcel_reader &r) {
      raw_storage<unpacked_of_t<Fn>> fn;

      unpacking<Fn>::unpack(r, &fn, std::false_type());

      fn.value.operator()();
      fn.destruct();
    }

  public:
    // Knowing the next thing on the reader was packed by command<>::pack, this
    // will pop it off and execute it leaving the reader pointing to just after
    // the command.
    static void execute(parcel_reader &r) {
      executor_t exec = r.pop_trivial_aligned<executor_t>();
      exec.fnptr_non_null()(r);
    }

    // Update an upper-bound on the parcel size needed to accomadate adding the
    // given callable as a command.
    template<typename Fn1, typename Ub,
             typename Fn = typename std::decay<Fn1>::type>
    static constexpr auto ubound(Ub ub0, Fn1 &&fn)
      -> decltype(
        packing<Fn>::ubound(ub0.template trivial_added<executor_t>(), fn, std::false_type())
      ) {
      return packing<Fn>::ubound(ub0.template trivial_added<executor_t>(), fn, std::false_type());
    }

    // Pack the given callable onto the writer.
    template<typename Fn1,
             typename Fn = typename std::decay<Fn1>::type>
    static void pack(parcel_writer &w, std::size_t size_ub, Fn1 &&fn) {
      w.template put_trivial_aligned<executor_t>(executor_t(&the_executor<Fn>));

      packing<Fn>::pack(w, fn, std::false_type());
      
      UPCXX_ASSERT(
        size_ub == 0 || w.size() <= size_ub,
        "Overflowed parcel buffer: buffer="<<size_ub<<", packed="<<w.size()
      ); // blame packing<Fn>::ubound()
    }
  };

  // command<Args...>: Non-empty args list specialization. The API is different
  // from the empty-args case. Now pack takes a `cleanup` template parameter
  // function which will be called with the executor-provided args after any
  // future returned by the given callable readies.
  template<typename ...Arg>
  class command {
    using executor_t = global_fnptr<void(parcel_reader&, Arg...)>;

    template<void(&cleanup)(Arg...)>
    struct after_execute {
      std::tuple<Arg...> a;
      template<typename ...T>
      void operator()(T&&...) {
        upcxx::apply_tupled(cleanup, std::move(a));
      }
    };
    
    template<typename Fn, void(&cleanup)(Arg...)>
    static void the_executor(parcel_reader &r, Arg ...a) {
      raw_storage<unpacked_of_t<Fn>> fn;

      unpacking<Fn>::unpack(r, &fn, std::false_type());

      upcxx::apply_as_future(fn.value_and_destruct())
        .then(after_execute<cleanup>{std::tuple<Arg...>{a...}});
    }

  public:
    // Knowing the next thing on the reader was packed by `command<Arg...>::pack`,
    // this will pop it off and execute it leaving the reader pointing to just
    // after the command. The `cleanup` function given to `pack` will execute
    // with the given arguments `a...` after the callable's returned future (if
    // any) is ready.
    static void execute(parcel_reader &r, Arg ...a) {
      executor_t exec = r.pop_trivial_aligned<executor_t>();
      exec.fnptr_non_null()(r, a...);
    }

    // Update an upper-bound on the parcel size needed to accomadate adding the
    // given callable as a command.
    template<typename Fn1, typename Ub,
             typename Fn = typename std::decay<Fn1>::type>
    static constexpr auto ubound(Ub ub0, Fn1 &&fn)
      -> decltype(
        packing<Fn>::ubound(ub0.template trivial_added<executor_t>(), fn, std::false_type())
      ) {
      return packing<Fn>::ubound(ub0.template trivial_added<executor_t>(), fn, std::false_type());
    }

    // Pack the given callable and cleanup action onto the writer.
    template<void(&cleanup)(Arg...), typename Fn1,
             typename Fn = typename std::decay<Fn1>::type>
    static void pack(parcel_writer &w, std::size_t size_ub, Fn1 &&fn) {
      w.template put_trivial_aligned<executor_t>(executor_t(&the_executor<Fn,cleanup>));

      packing<Fn>::pack(w, fn, std::false_type());
      
      UPCXX_ASSERT(
        size_ub == 0 || w.size() <= size_ub,
        "Overflowed parcel buffer: buffer="<<size_ub<<", packed="<<w.size()
      ); // blame packing<Fn>::ubound()
    }
  };
}
#endif
