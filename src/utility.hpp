#ifndef _661bba4d_9f90_4fbe_b617_4474e1ed8cab
#define _661bba4d_9f90_4fbe_b617_4474e1ed8cab

#include <upcxx/diagnostic.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <iostream>
#include <sstream>
#include <tuple>
#include <utility>

namespace upcxx {
  //////////////////////////////////////////////////////////////////////
  // nop_function
  
  template<typename Sig>
  struct nop_function;
  
  template<typename Ret, typename ...Arg>
  struct nop_function<Ret(Arg...)> {
    template<typename ...Arg1>
    Ret operator()(Arg1 &&...a) const {
      UPCXX_ASSERT(false);
      throw std::bad_function_call();
    }
  };
  template<typename ...Arg>
  struct nop_function<void(Arg...)> {
    template<typename ...Arg1>
    void operator()(Arg1 &&...a) const {}
  };
  
  template<typename Sig>
  nop_function<Sig> nop() {
    return nop_function<Sig>{};
  }
  
  //////////////////////////////////////////////////////////////////////
  // constant_function
  
  template<typename T>
  struct constant_function {
    T value_;
    constant_function(T value): value_{std::move(value)} {}
    
    template<typename ...Arg>
    T operator()(Arg &&...args) const {
      return value_;
    }
  };
  
  template<typename T>
  inline constant_function<T> constant(T value) {
    return constant_function<T>{std::move(value)};
  }
  
  //////////////////////////////////////////////////////////////////////
  // function_ref: reference to a function. Useful when you want to pass
  // a lambda into a subroutine knowing the lambda won't be used after
  // the subroutine exits. A regular std::function could be used in this
  // case but this has the advantage of doing no heap allocations and
  // no virtual call for the destructor.
  //
  // void my_foreach(int n, function_ref<void(int)> fn) {
  //   for(int i=0; i < n; i++)
  //     fn(i);
  // }
  //
  // my_foreach(10, [=](int i) { cout << "i="<<i<<'\n'; });
  
  template<typename Sig>
  class function_ref;
  
  template<typename Ret, typename ...Arg>
  class function_ref<Ret(Arg...)> {
    Ret(*invoker_)(Arg...);
    void *fn_;
    
  private:
    template<typename Fn>
    static Ret the_invoker(void *fn, Arg ...arg) {
      return reinterpret_cast<Fn*>(fn)->operator()(static_cast<Arg>(arg)...);
    }
    
    static Ret the_nop_invoker(void *fn, Arg ...arg) {
      return nop_function<Ret(Arg...)>{}();
    }
    
  public:
    function_ref():
      invoker_{the_nop_invoker},
      fn_{nullptr} {
    }
    template<typename Fn>
    function_ref(Fn &&fn):
      invoker_{the_invoker<typename std::remove_reference<Fn>::type>},
      fn_{reinterpret_cast<void*>(const_cast<Fn*>(&fn))} {
    }
    function_ref(const function_ref&) = default;
    function_ref& operator=(const function_ref&) = default;
    function_ref(function_ref&&) = default;
    function_ref& operator=(function_ref&&) = default;
    
    Ret operator()(Arg ...arg) const {
      return invoker_(fn_, static_cast<Arg>(arg)...);
    }
  };
  
  //////////////////////////////////////////////////////////////////////

  template<template<typename> class Test, typename ...T>
  struct trait_forall;
  template<template<typename> class Test>
  struct trait_forall<Test> {
    static constexpr bool value = true;
  };
  template<template<typename> class Test, typename T, typename ...Ts>
  struct trait_forall<Test,T,Ts...> {
    static constexpr bool value = Test<T>::value && trait_forall<Test,Ts...>::value;
  };
  
  //////////////////////////////////////////////////////////////////////

  template<typename Tuple, template<typename...> class Into>
  struct tuple_types_into;
  template<typename ...T, template<typename...> class Into>
  struct tuple_types_into<std::tuple<T...>, Into> {
    typedef Into<T...> type;
  };
  template<typename Tuple, template<typename...> class Into>
  using tuple_types_into_t = typename tuple_types_into<Tuple,Into>::type;
  
  //////////////////////////////////////////////////////////////////////

  template<int...>
  struct index_sequence {};

  //////////////////////////////////////////////////////////////////////

  template<int n, int ...s>
  struct _make_index_sequence: _make_index_sequence<n-1, n-1, s...> {};

  template<int ...s>
  struct _make_index_sequence<0, s...> {
    typedef index_sequence<s...> type;
  };

  template<int n>
  using make_index_sequence = typename _make_index_sequence<n>::type;

  //////////////////////////////////////////////////////////////////////

  template<typename Tup>
  struct tuple_decay;
  template<typename ...T>
  struct tuple_decay<std::tuple<T...>> {
    typedef std::tuple<typename std::decay<T>::type...> type;
  };
    
  template<typename ...T>
  std::tuple<const T&...> tuple_crefs(const std::tuple<T...> &x) {
    return std::tuple<const T&...>(x);
  }
  
  namespace detail {
    template<typename ...T, int ...i>
    std::tuple<T&...> tuple_refs(std::tuple<T...> &x, index_sequence<i...>) {
      return std::tuple<T&...>{std::get<i>(x)...};
    }
    
    template<typename ...T, int ...i>
    std::tuple<T&&...> tuple_rrefs(std::tuple<T...> &x, index_sequence<i...>) {
      return std::tuple<T&&...>{static_cast<T&&>(std::get<i>(x))...};
    }
    template<typename ...T, int ...i>
    std::tuple<T&&...> tuple_rrefs(std::tuple<T&...> x, index_sequence<i...>) {
      return std::tuple<T&&...>{static_cast<T&&>(std::get<i>(x))...};
    }
  }
  
  //////////////////////////////////////////////////////////////////////
  // tuple_refs: Get individual references to tuple componenets.
  
  template<typename ...T>
  std::tuple<T&...> tuple_refs(std::tuple<T...> &x) {
    return detail::tuple_refs(x, make_index_sequence<sizeof...(T)>());
  }
  template<typename ...T>
  std::tuple<T&...> tuple_refs(std::tuple<T&...> x) {
    return x;
  }
  inline std::tuple<> tuple_refs(std::tuple<>) {
    return std::tuple<>{};
  }
  
  //////////////////////////////////////////////////////////////////////
  // tuple_rvalues: Get rvalue-references to tuple componenets if
  // possible, otherwise just values.
  
  template<typename ...T>
  std::tuple<T...> tuple_rvalues(std::tuple<T...> x) {
    return std::move(x);
  }
  template<typename ...T>
  std::tuple<T&&...> tuple_rvalues(std::tuple<T...> &x) {
    return detail::tuple_rrefs(x, make_index_sequence<sizeof...(T)>());
  }
  template<typename ...T>
  std::tuple<T&&...> tuple_rvalues(std::tuple<T&...> x) {
    return detail::tuple_rrefs(x, make_index_sequence<sizeof...(T)>());
  }
  template<typename ...T>
  std::tuple<T&&...> tuple_rvalues(std::tuple<T&&...> x) {
    return std::move(x);
  }
  inline std::tuple<> tuple_rvalues(std::tuple<>) {
    return std::tuple<>{};
  }
  
  //////////////////////////////////////////////////////////////////////

  template<typename Fn, typename Tup, int ...i>
  inline auto apply_tupled_impl(Fn &&fn, Tup &&args, index_sequence<i...>) 
    -> decltype(fn(std::get<i>(args)...)) {
    return fn(std::get<i>(args)...);
  }
      
  template<typename Fn, typename Tup>
  inline auto apply_tupled(Fn &&fn, Tup &&args)
    -> decltype(
      apply_tupled_impl(
        std::forward<Fn>(fn), std::forward<Tup>(args),
        make_index_sequence<std::tuple_size<Tup>::value>()
      )
    ) {
    return apply_tupled_impl(
      std::forward<Fn>(fn), std::forward<Tup>(args),
      make_index_sequence<std::tuple_size<Tup>::value>()
    );
  }
}

#endif
