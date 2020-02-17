#ifndef _7b2d1734_0520_46ad_9c2e_bb2fec19144b
#define _7b2d1734_0520_46ad_9c2e_bb2fec19144b


/**
 * memberof.hpp
 */

#include <upcxx/global_ptr.hpp>
#include <upcxx/diagnostic.hpp>
#include <upcxx/memory_kind.hpp>
#include <upcxx/rpc.hpp>
#include <upcxx/future.hpp>

#include <cstddef> // ptrdiff_t
#include <cstdint> // uintptr_t
#include <type_traits>

// UPCXX_GPTYPE(global_ptr<T> gp) yields type global_ptr<T> for any expression gp
#define UPCXX_GPTYPE(gp) \
  ::std::remove_reference<decltype(gp)>::type

// UPCXX_ETYPE(global_ptr<E> gp) yields typename E for any expression gp
#define UPCXX_ETYPE(gp)  typename UPCXX_GPTYPE(gp)::element_type

// UPCXX_KTYPE(global_ptr<E,kind> gp) yields kind for any expression gp
#define UPCXX_KTYPE(gp)  (UPCXX_GPTYPE(gp)::kind)

// UNSPECIFIED MACRO: This variant is not guaranteed by the spec
// upcxx_memberof_unsafe(global_ptr<T> gp, field-designator)
// This variant assumes T is standard layout, or (C++17) is conditionally supported by the compiler for use in offsetof
// Otherwise, the result is undefined behavior
#define upcxx_memberof_unsafe(gp, FIELD) ( \
  UPCXX_STATIC_ASSERT(offsetof(UPCXX_ETYPE(gp), FIELD) < sizeof(UPCXX_ETYPE(gp)), \
                      "offsetof returned a bogus result. This is probably due to an unsupported non-standard-layout type"), \
  ::upcxx::global_ptr<decltype(::std::declval<UPCXX_ETYPE(gp)>().FIELD), UPCXX_KTYPE(gp)>( \
    ::upcxx::detail::internal_only(), \
    (gp),\
    offsetof(UPCXX_ETYPE(gp), FIELD) \
    ) \
  )
    
// upcxx_memberof(global_ptr<T> gp, field-designator)
// This variant asserts T is standard layout, and thus guaranteed by C++11 to produce well-defined results
#define upcxx_memberof(gp, FIELD) ( \
    UPCXX_STATIC_ASSERT(::std::is_standard_layout<UPCXX_ETYPE(gp)>::value, \
     "upcxx_memberof() requires a global_ptr to a standard-layout type. Perhaps you want upcxx_memberof_unsafe()?"), \
     upcxx_memberof_unsafe(gp, FIELD) \
  )

namespace upcxx { namespace detail {

template<typename Obj, memory_kind Kind, typename Mbr, typename Get,
         bool standard_layout = std::is_standard_layout<Obj>::value>
struct memberof_general_dispatch;

template<typename Obj, memory_kind Kind, typename Mbr, typename Get>
struct memberof_general_dispatch<Obj, Kind, Mbr, Get, /*standard_layout=*/true> {
  decltype(upcxx::make_future(std::declval<global_ptr<Mbr,Kind>>()))
  operator()(global_ptr<Obj,Kind> gptr, Get getter) const {
    return upcxx::make_future(
            global_ptr<Mbr,Kind>(detail::internal_only(), gptr.rank_, 
                                 getter(gptr.raw_ptr_), gptr.device_));
  }
};

template<typename Obj, memory_kind Kind, typename Mbr, typename Get>
struct memberof_general_dispatch<Obj, Kind, Mbr, Get, /*standard_layout=*/false> {
  future<global_ptr<Mbr,Kind>> 
  operator()(global_ptr<Obj,Kind> gptr, Get getter) const {
    return upcxx::rpc(gptr.rank_, [=]() {
      return global_ptr<Mbr,Kind>(detail::internal_only(), gptr.rank_, 
                                  getter(gptr.raw_ptr_), gptr.device_);
    });
  }
};

// Infers all template parameters
template<typename Obj, memory_kind Kind, typename Get, 
         typename Mbr = typename std::remove_pointer<decltype(std::declval<Get>()(std::declval<Obj*>()))>::type>
auto memberof_general_helper(global_ptr<Obj,Kind> gptr, Get getter)
  -> decltype(memberof_general_dispatch<Obj,Kind,Mbr,Get>()(gptr, getter)) {
  UPCXX_ASSERT(gptr, "Global pointer expression to upcxx_memberof_general() may not be null");
  return memberof_general_dispatch<Obj,Kind,Mbr,Get>()(gptr, getter);
}

}} // namespace upcxx::detail

#define upcxx_memberof_general(gp, FIELD) ( \
  ::upcxx::detail::memberof_general_helper((gp), \
    [](UPCXX_ETYPE(gp) *lptr) { return ::std::addressof(lptr->FIELD); } \
  ) \
)


#endif
