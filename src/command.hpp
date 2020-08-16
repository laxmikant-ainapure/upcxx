#ifndef _3589ecdd_36fa_4240_ade2_c807b184ac7f
#define _3589ecdd_36fa_4240_ade2_c807b184ac7f

#include <upcxx/diagnostic.hpp>
#include <upcxx/future.hpp>
#include <upcxx/global_fnptr.hpp>
#include <upcxx/serialization.hpp>

// Commands are callable objects that have been packed into a parcel.

namespace upcxx {
namespace detail {
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
        detail::apply_tupled(cleanup, std::move(a));
      }
    };

#ifndef UPCXX_RPC_STACK_MAX_FN_SIZE
#define UPCXX_RPC_STACK_MAX_FN_SIZE 2048
#endif

    // Fn is stored on the stack if it is less than the size limit and
    // does not return a future.
    // - If Fn is too big, store it on the heap to avoid smashing the
    //   stack.
    // - If Fn returns a future, we need to extend its lifetime (along
    //   with its bound arguments) until the future is ready. So we
    //   store it on the heap and destroy it after the future is ready.
    template<typename Fn,
             typename Return = typename std::result_of<Fn&&()>::type,
             bool unboxed = sizeof(Fn) <= UPCXX_RPC_STACK_MAX_FN_SIZE>
    struct cmd_storage:
      detail::raw_storage<typename serialization_traits<Fn>::deserialized_type> {
      using storage_t =
        detail::raw_storage<typename serialization_traits<Fn>::deserialized_type>;
      storage_t *ptr() {
        return this;
      }
      // cleanup done with a call to destruct(), so nothing to do here
      template<typename Kind>
      void attach_cleanup(future1<Kind> done) {}
    };

    template<typename Fn, typename Return>
    struct cmd_storage<Fn, Return, false> { // too big
      using storage_t =
        detail::raw_storage<typename serialization_traits<Fn>::deserialized_type>;
      storage_t *fnptr = ::new storage_t;
      storage_t *ptr() {
        return fnptr;
      }
      // cleanup must be deferred, so destruct() does nothing
      void destruct() {}
      template<typename Kind>
      void attach_cleanup(future1<Kind> done) {
        storage_t *fnptr = this->fnptr;
        done.then([fnptr]() { // attach the actual cleanup to done
                    fnptr->destruct();
                    ::delete fnptr;
                  });
      }
    };

    template<typename Fn, typename Kind, typename ...T>
    struct cmd_storage<Fn, future1<Kind, T...>, true>: // returns a future
      cmd_storage<Fn, future1<Kind, T...>, false> {};

    template<typename Fn, detail::serialization_reader(*reader)(Arg...), void(*cleanup)(Arg...)>
    static void the_executor(Arg ...a) {
      detail::serialization_reader r = reader(a...);
      
      r.template read_trivial<executor_wire_t>();

      cmd_storage<Fn> storage;
      serialization_traits<Fn>::deserialize(r, storage.ptr());

      storage.attach_cleanup(
        upcxx::apply_as_future(std::move(storage.ptr()->value()))
          .then(after_execute<cleanup>{std::tuple<Arg...>(a...)})
      );

      storage.destruct();
    }

  public:
    using executor_t = void(*)(Arg...);
    
    // Given a reader in the same state as the one passed into `command::serialize`,
    // this will retrieve the executor function.
    static executor_t get_executor(detail::serialization_reader r) {
      executor_wire_t exec = r.template read_trivial<executor_wire_t>();
      UPCXX_ASSERT(exec.u_ != 0);
      return exec.fnptr_non_null();
    }

    // Update an upper-bound on the size needed to accomadate adding the
    // given callable as a command.
    template<typename Fn1, typename SS,
             typename Fn = typename std::decay<Fn1>::type>
    static constexpr auto ubound(SS ub0, Fn1 &&fn)
      -> decltype(
        ub0.template cat_size_of<executor_wire_t>()
           .template cat_ubound_of<Fn>(fn)
      ) {
      return ub0.template cat_size_of<executor_wire_t>()
                .template cat_ubound_of<Fn>(fn);
    }

    // Serialize the given callable and reader and cleanup actions onto the writer.
    template<detail::serialization_reader(*reader)(Arg...), void(*cleanup)(Arg...),
             typename Fn1, typename Writer,
             typename Fn = typename std::decay<Fn1>::type>
    static void serialize(Writer &w, std::size_t size_ub, Fn1 &&fn) {
      executor_wire_t exec = executor_wire_t(&the_executor<Fn,reader,cleanup>);
      
      w.template write_trivial<executor_wire_t>(exec);
      
      serialization_traits<Fn>::serialize(w, fn);
      
      UPCXX_ASSERT(
        size_ub == 0 || w.size() <= size_ub,
        "Overflowed serialization buffer: buffer="<<size_ub<<", packed="<<w.size()
      ); // blame serialization<Fn>::ubound()
    }
  };
}}
#endif
