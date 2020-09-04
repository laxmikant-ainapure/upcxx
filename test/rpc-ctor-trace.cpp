#include <upcxx/upcxx.hpp>
#include "util.hpp"

struct T {
  static int ctors, dtors, copies, moves;
  static void show_stats(char const *title, int expected_ctors, int expected_copies,
                         int expected_moves=-1);
  
  bool valid = true;
  T() { ctors++; }
  T(T const &that) {
    UPCXX_ASSERT_ALWAYS(that.valid, "copying from an invalidated object");
    copies++;
  }
  T(T &&that) {
    UPCXX_ASSERT_ALWAYS(that.valid, "moving from an invalidated object");
    that.valid = false;
    moves++;
  }
  ~T() {
    valid = false;
    dtors++;
  }

  UPCXX_SERIALIZED_FIELDS(valid)
};

int T::ctors = 0;
int T::dtors = 0;
int T::copies = 0;
int T::moves = 0;

void T::show_stats(const char *title, int expected_ctors, int expected_copies,
                   int expected_moves) {
  upcxx::barrier();
  
  if(upcxx::rank_me() == 0) {
    std::cout<<title<<std::endl;
    std::cout<<"  T::ctors = "<<ctors<<std::endl;
    std::cout<<"  T::copies = "<<copies<<std::endl;
    std::cout<<"  T::moves = "<<moves<<std::endl;
    std::cout<<"  T::dtors = "<<dtors<<std::endl;
    std::cout<<std::endl;
  }

  #if 1
  UPCXX_ASSERT_ALWAYS(ctors == expected_ctors, title<<": ctors="<<ctors<<" expected="<<expected_ctors);
  UPCXX_ASSERT_ALWAYS(copies == expected_copies, title<<": copies="<<copies<<" expected="<<expected_copies);
  UPCXX_ASSERT_ALWAYS(expected_moves == -1 || moves == expected_moves,
                      title<<": moves="<<moves<<" expected="<<expected_moves);
  UPCXX_ASSERT_ALWAYS(ctors+copies+moves == dtors, title<<": ctors - dtors != 0");
  #endif
  
  ctors = copies = moves = dtors = 0;

  upcxx::barrier();
}

T global;

bool done = false;

struct Fn { // movable and copyable function object
  T t;
  void operator()() { done = true; }
  UPCXX_SERIALIZED_FIELDS(t)
};

struct NmNcFn { // non-movable/non-copyable function object
  T t;
  void operator()() { done = true; }
  NmNcFn() {}
  NmNcFn(const NmNcFn&) = delete;
  UPCXX_SERIALIZED_FIELDS(t)
};

int main() {
  upcxx::init();
  print_test_header();

  --T::ctors; // discount construction of global

  using upcxx::dist_object;
  dist_object<int> dob(3);

  int target = (upcxx::rank_me() + 1) % upcxx::rank_n();

  // About the expected num of copies.
  // Now that backend::send_awaken_lpc can take a std::tuple
  // containing a reference to T and serialize from that place, it no
  // longer involves an extra copy in the future<T> returning cases.
  
  upcxx::rpc(target,
    [](T &&x) {
    },
    T()
  ).wait_reference();
  T::show_stats("T&& ->", 2, 0, 0);

  upcxx::rpc(target,
    [](T const &x) {
    },
    global
  ).wait_reference();
  T::show_stats("T& ->", 1, 0, 0);

  upcxx::rpc(target,
    [](T const &x) {
    },
    static_cast<T const&>(global)
  ).wait_reference();
  T::show_stats("T const& ->", 1, 0, 0);

  upcxx::rpc(target,
    []() -> T {
      return T();
    }
  ).wait_reference();
  T::show_stats("-> T", 2, 0, 4);

  upcxx::rpc(target,
    [](T &&x) -> T {
      return std::move(x);
    },
    T()
  ).wait_reference();
  T::show_stats("T&& -> T", 3, 0, 5);

  upcxx::rpc(target,
    [](T const &x) -> T {
      return x;
    },
    static_cast<T const&>(global)
  ).wait_reference();
  T::show_stats("T const& -> T", 2, 1, 4);

  upcxx::rpc(target,
    [](T &&x) -> upcxx::future<T> {
      return upcxx::make_future(std::move(x));
    },
    T()
  ).wait_reference();
  T::show_stats("T&& -> future<T>", 3, 0, 5);

  upcxx::rpc(target,
    [](T const &x) -> upcxx::future<T> {
      return upcxx::make_future(x);
    },
    static_cast<T const&>(global)
  ).wait_reference();
  T::show_stats("T const& -> future<T>", 2, 1, 4);

  // now with dist_object

  {
    dist_object<T> dobT(upcxx::world());
    dobT.fetch(target).wait_reference();
    upcxx::barrier();
  }
  T::show_stats("dist_object<T>::fetch()", 2, 0, 2);

  upcxx::rpc(target,
    [](dist_object<int>&, T &&x) -> T {
      return std::move(x);
    },
    dob, T()
  ).wait_reference();
  T::show_stats("dist_object + T&& -> T", 3, 0, 5);

  upcxx::rpc(target,
    [](dist_object<int>&, T const &x) -> T {
      return x;
    },
    dob, static_cast<T const&>(global)
  ).wait_reference();
  T::show_stats("dist_object + T const& -> T", 2, 1, 4);

  upcxx::rpc(target,
    [](dist_object<int>&, T &&x) -> upcxx::future<T> {
      return upcxx::make_future(std::move(x));
    },
    dob, T()
  ).wait_reference();
  T::show_stats("dist_object + T&& -> future<T>", 3, 0, 5);

  upcxx::rpc(target,
    [](dist_object<int>&, T const &x) -> upcxx::future<T> {
      return upcxx::make_future(x);
    },
    dob, static_cast<T const&>(global)
  ).wait_reference();
  T::show_stats("dist_object + T const& -> future<T>", 2, 1, 4);

  // returning references

  upcxx::rpc(target,
    [](T &&x) -> T&& {
      return std::move(x);
    },
    T()
  ).wait_reference();
  T::show_stats("T&& -> T&&", 3, 0, 2);

  upcxx::rpc(target,
    [](T const &x) -> T& {
      return global;
    },
    global
  ).wait_reference();
  T::show_stats("T& -> T&", 2, 0, 2);

  upcxx::rpc(target,
    [](T const &x) -> T const& {
      return x;
    },
    static_cast<T const&>(global)
  ).wait_reference();
  T::show_stats("T const& -> T const&", 2, 0, 2);

  upcxx::rpc(target,
    [](T const &x) -> upcxx::future<T const&> {
      return upcxx::make_future<T const&>(x);
    },
    static_cast<T const&>(global)
  ).wait_reference();
  T::show_stats("T const& -> future<T const&>", 2, 0, 2);

  upcxx::rpc(target,
    [](upcxx::view<T> v) -> T const& {
      auto storage =
        new typename std::aligned_storage <sizeof(T),
                                           alignof(T)>::type;
      T *p = v.begin().deserialize_into(storage);
      delete p;
      return global;
    },
    upcxx::make_view(&global, &global+1)
  ).wait_reference();
  T::show_stats("view<T> -> T const&", 2, 0, 2);

  upcxx::rpc(target,
    []() -> T& {
      return global;
    }
  ).wait_reference();
  T::show_stats("-> T&", 1, 0, 2);

  upcxx::rpc(target,
    []() -> T const& {
      return global;
    }
  ).wait_reference();
  T::show_stats("-> T const&", 1, 0, 2);

  // function object

  {
    NmNcFn fn;
    upcxx::rpc(target, fn).wait_reference();
  }
  T::show_stats("NmNcFn& ->", 3, 0, 0);

  upcxx::rpc(target, NmNcFn()).wait_reference();
  T::show_stats("NmNcFn&& ->", 3, 0, 0);

  // rpc_ff

  upcxx::barrier();
  done = false;
  upcxx::barrier();

  upcxx::rpc_ff(target,
    [](T &&x) {
      done = true;
    },
    T()
  );
  while (!done) { upcxx::progress(); }
  done = false;
  upcxx::barrier();
  T::show_stats("(rpc_ff) T&& ->", 2, 0, 0);

  upcxx::rpc_ff(target,
    [](T const &x) {
      done = true;
    },
    global
  );
  while (!done) { upcxx::progress(); }
  done = false;
  upcxx::barrier();
  T::show_stats("(rpc_ff) T& ->", 1, 0, 0);

  upcxx::rpc_ff(target,
    [](T const &x) {
      done = true;
    },
    static_cast<T const&>(global)
  );
  while (!done) { upcxx::progress(); }
  done = false;
  upcxx::barrier();
  T::show_stats("(rpc_ff) T const& ->", 1, 0, 0);

  {
    Fn fn;
    upcxx::rpc_ff(target, fn);
  }
  while (!done) { upcxx::progress(); }
  done = false;
  upcxx::barrier();
  T::show_stats("(rpc_ff) Fn& ->", 3, 0, 0);

  upcxx::rpc_ff(target, Fn());
  while (!done) { upcxx::progress(); }
  done = false;
  upcxx::barrier();
  T::show_stats("(rpc_ff) Fn&& ->", 3, 0, 0);

  print_test_success();
  upcxx::finalize();
}
