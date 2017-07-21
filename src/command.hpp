#ifndef _3589ecdd_36fa_4240_ade2_c807b184ac7f
#define _3589ecdd_36fa_4240_ade2_c807b184ac7f

#include <upcxx/diagnostic.hpp>
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
  // Bound the size of the packed callable.
  template<typename Fn>
  void command_size_ubound(parcel_layout &ub, Fn &&fn);
  
  // Pack the callable.
  template<typename Fn>
  void command_pack(parcel_writer &w, Fn &&fn, std::size_t size_ub=0);
  
  // Assuming the next thing on the reader is a command, unpacks
  // and executes it.
  void command_unpack_and_execute(parcel_reader &r);
  
  
  //////////////////////////////////////////////////////////////////////
  // implementation
  
  namespace detail {
    template<typename FnUnpacker>
    void command_unpack_and_executer(parcel_reader &r) {
      packing<FnUnpacker>::unpack(r)();
    }
  }
  
  inline void command_unpack_and_execute(parcel_reader &r) {
    typedef void(*unpex_t)(parcel_reader&);
    packing<unpex_t>::unpack(r)(r);
  }
  
  template<typename Fn1>
  inline void command_size_ubound(parcel_layout &ub, Fn1 &&fn) {
    typedef typename std::decay<Fn1>::type Fn;
    typedef typename unpacking<Fn>::type FnUnpacker;
    typedef void(*unpex_t)(parcel_reader&);
    
    unpex_t unpex = detail::command_unpack_and_executer<FnUnpacker>;
    packing<unpex_t>::size_ubound(ub, unpex);
    packing<Fn>::size_ubound(ub, fn);
  }
  
  template<typename Fn1>
  inline void command_pack(parcel_writer &w, Fn1 &&fn, std::size_t size_ub) {
    typedef typename std::decay<Fn1>::type Fn;
    typedef typename unpacking<Fn>::type FnUnpacker;
    typedef void(*unpex_t)(parcel_reader&);
    
    unpex_t unpex = detail::command_unpack_and_executer<FnUnpacker>;
    packing<unpex_t>::pack(w, unpex);
    packing<Fn>::pack(w, fn);
    
    UPCXX_ASSERT(
      size_ub == 0 || w.layout().size() <= size_ub,
      "Overflowed parcel buffer: buffer="<<size_ub<<", packed="<<w.layout().size()
    ); // blame packing<Fn>::size_ubound
  }
}
#endif
