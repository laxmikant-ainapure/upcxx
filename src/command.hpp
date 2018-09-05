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
  class command {
    using executor_wire_t = global_fnptr<void(Arg...)>;
    
    template<void(*cleanup)(Arg...)>
    struct after_execute {
      std::tuple<Arg...> a;
      template<typename ...T>
      void operator()(T&&...) {
        upcxx::apply_tupled(cleanup, std::move(a));
      }
    };
    
    template<typename Fn, parcel_reader(*reader)(Arg...), void(*cleanup)(Arg...)>
    static void the_executor(Arg ...a) {
      parcel_reader r = reader(a...);
      
      r.pop_trivial_aligned<executor_wire_t>();
      
      raw_storage<unpacked_of_t<Fn>> fn;
      unpacking<Fn>::unpack(r, &fn, /*skippable=*/std::false_type());
      
      upcxx::apply_as_future(fn.value_and_destruct())
        .then(after_execute<cleanup>{std::tuple<Arg...>(a...)});
    }

  public:
    using executor_t = void(*)(Arg...);
    
    // Given a reader in the same state as the one passed into `command::pack`,
    // this will retrieve the executor function.
    static executor_t get_executor(parcel_reader r) {
      executor_wire_t exec = r.pop_trivial_aligned<executor_wire_t>();
      UPCXX_ASSERT(exec.u_ != 0);
      return exec.fnptr_non_null();
    }

    // Update an upper-bound on the parcel size needed to accomadate adding the
    // given callable as a command.
    template<typename Fn1, typename Ub,
             typename Fn = typename std::decay<Fn1>::type>
    static constexpr auto ubound(Ub ub0, Fn1 &&fn)
      -> decltype(
        packing<Fn>::ubound(ub0.template trivial_added<executor_wire_t>(), fn, std::false_type())
      ) {
      return packing<Fn>::ubound(ub0.template trivial_added<executor_wire_t>(), fn, std::false_type());
    }

    // Pack the given callable and reader and cleanup actions onto the writer.
    template<parcel_reader(*reader)(Arg...), void(*cleanup)(Arg...),
             typename Fn1,
             typename Fn = typename std::decay<Fn1>::type>
    static void pack(parcel_writer &w, std::size_t size_ub, Fn1 &&fn) {
      executor_wire_t exec = executor_wire_t(&the_executor<Fn,reader,cleanup>);
      
      w.template put_trivial_aligned<executor_wire_t>(exec);
      
      packing<Fn>::pack(w, fn, /*skippable=*/std::false_type());
      
      UPCXX_ASSERT(
        size_ub == 0 || w.size() <= size_ub,
        "Overflowed parcel buffer: buffer="<<size_ub<<", packed="<<w.size()
      ); // blame packing<Fn>::ubound()
    }
  };
}
#endif
