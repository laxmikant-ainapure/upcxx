#ifndef _3589ecdd_36fa_4240_ade2_c807b184ac7f
#define _3589ecdd_36fa_4240_ade2_c807b184ac7f

#include <upcxx/diagnostic.hpp>
#include <upcxx/future.hpp>
#include <upcxx/packing.hpp>

/* Commands are callable object that have been packed into a parcel.
 * Packing a command is as straight forward as packing the object, but
 * unpacking back into a value is no longer possible. Instead, the
 * unpack and execute happen as a single operation. This has the semantic
 * niceness of guaranteeing the buffer from which we unpack will stay
 * alive for the duration of the execution. This helps us upsteam when
 * crafting protocols for streaming data through commands, by letting us
 * avoid having pedantic copy-outs of arrays/etc into their own
 * value buffer just so they can be deposited somewhere else.
 */

namespace upcxx {
  template<typename Fn>
  struct commanding/*{
    // Identical to packing<T> counterparts.
    static void size_ubound(parcel_layout &ub, Fn const&);
    static void pack(parcel_writer &w, Fn const&);
    
    // Directly execute an Fn out of the reader assuming its the
    // next object. Returned future indicates release of underlying
    // parcel buffer.
    static future<> execute(parcel_reader &r);
  }*/;
  
  // General case defers to packing<T> and assumes buffer is not needed
  // after execution returns.
  template<typename Fn>
  struct commanding {
    static void size_ubound(parcel_layout &ub, Fn const &x) {
      packing<Fn>::size_ubound(ub, x);
    }
    
    static void pack(parcel_writer &w, Fn const &x) {
      packing<Fn>::pack(w, x);
    }
    
    static auto execute(parcel_reader &r)
      -> decltype(make_future()) {
      packing<Fn>::unpack(r)();
      return make_future();
    }
  };
  
  // Bound the size of the packed callable.
  template<typename Fn>
  void command_size_ubound(parcel_layout &ub, Fn &&fn);
  
  // Pack the callable.
  template<typename Fn>
  void command_pack(parcel_writer &w, std::size_t size_ub, Fn &&fn);
  
  // Assuming the next thing on the reader is a command, unpacks
  // and executes it. Returned future indicates client's release of 
  // underlying parcel buffer.
  future<> command_execute(parcel_reader &r);
  
  //////////////////////////////////////////////////////////////////////
  // implementation
  
  namespace detail {
    template<typename Fn>
    future<> command_executor(parcel_reader &r) {
      return commanding<Fn>::execute(r);
    }
  }
  
  inline future<> command_execute(parcel_reader &r) {
    typedef future<>(*exec_t)(parcel_reader&);
    return packing<exec_t>::unpack(r)(r);
  }
  
  template<typename Fn1>
  void command_size_ubound(parcel_layout &ub, Fn1 &&fn) {
    typedef typename std::decay<Fn1>::type Fn;
    typedef future<>(*exec_t)(parcel_reader&);
    
    exec_t exec = detail::command_executor<Fn>;
    packing<exec_t>::size_ubound(ub, exec);
    commanding<Fn>::size_ubound(ub, fn);
  }
  
  template<typename Fn1>
  void command_pack(
      parcel_writer &w, std::size_t size_ub,
      Fn1 &&fn
    ) {
    typedef typename std::decay<Fn1>::type Fn;
    typedef future<>(*exec_t)(parcel_reader&);
    
    exec_t exec = detail::command_executor<Fn>;
    packing<exec_t>::pack(w, exec);
    commanding<Fn>::pack(w, fn);
    
    UPCXX_ASSERT(
      size_ub == 0 || w.layout().size() <= size_ub,
      "Overflowed parcel buffer: buffer="<<size_ub<<", packed="<<w.layout().size()
    ); // blame commanding<Fn>::size_ubound()
  }
}
#endif
