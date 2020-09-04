#include <iomanip>
#include <upcxx/upcxx.hpp>
#include "util.hpp"

// This test measures the number of copies/moves invoked on objects passed to
// various UPC++ routines. The results asserted by this test are only indicative
// of the current implementation and should NOT be construed as a guarantee of
// future copy/move behavior. 
// Consult the UPC++ Specification for guaranteed copy/move behaviors.

struct T {
  static int ctors, dtors, copies, moves;
  static void show_stats(int line, char const *title, int expected_ctors, int expected_copies,
                         int expected_moves=-1);
  static void reset_counts() { ctors = copies = moves = dtors = 0; } 
  
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

bool success = true;

void T::show_stats(int line, const char *title, int expected_ctors, int expected_copies,
                   int expected_moves) {
  upcxx::barrier();
  
  #if !SKIP_OUTPUT
  if(upcxx::rank_me() == 0) {
    std::cout<<std::left<<std::setw(50)<<title<< " \t(line " << line << ")" << std::endl;
    std::cout<<"  T::ctors  = "<<ctors<<std::endl;
    std::cout<<"  T::copies = "<<copies<<std::endl;
    std::cout<<"  T::moves  = "<<moves<<std::endl;
    std::cout<<"  T::dtors  = "<<dtors<<std::endl;
    std::cout<<std::endl;
  }
  #endif

  #define CHECK(prop, ...) do { \
    if (!(prop)) { \
      success = false; \
      if (!upcxx::rank_me()) \
        std::cerr << "ERROR: failed check: " << #prop << "\n" \
                  << title << ": " << __VA_ARGS__ << "\n" << std::endl; \
    } \
  } while (0)
  CHECK(ctors == expected_ctors, "ctors="<<ctors<<" expected="<<expected_ctors);
  CHECK(copies == expected_copies, "copies="<<copies<<" expected="<<expected_copies);
  CHECK(expected_moves == -1 || moves == expected_moves,
                      "moves="<<moves<<" expected="<<expected_moves);
  CHECK(ctors+copies+moves == dtors, "ctors - dtors != 0");
 
  T::reset_counts();

  upcxx::barrier();
}
#define SHOW(...) T::show_stats(__LINE__, __VA_ARGS__)

T global;

bool done = false;

struct Fn {
  T t;
  void operator()() { done = true; }
  UPCXX_SERIALIZED_FIELDS(t)
};

int main() {
  upcxx::init();
  print_test_header();

  T::reset_counts(); // discount construction of global

  int peer = (upcxx::rank_me() + 1) % upcxx::rank_n();
  upcxx::persona &target = upcxx::current_persona();

  // lpc
  { 
    Fn fn;
    auto f = target.lpc(fn);
    f.wait_reference();
  }
  done = false;
  SHOW("lpc(Fn&) ->", 1, 1, 1);

  { 
    auto f = target.lpc(Fn());
    f.wait_reference();
  }
  done = false;
  SHOW("lpc(Fn&&) ->", 1, 0, 2);

  // lpc_ff
  { 
    Fn fn;
    target.lpc_ff(fn);
    while (!done) { upcxx::progress(); }
  }
  done = false;
  SHOW("lpc_ff(Fn&) ->", 1, 1, 0);

  { 
    target.lpc_ff(Fn());
    while (!done) { upcxx::progress(); }
  }
  done = false;
  SHOW("lpc_ff(Fn&&) ->", 1, 0, 1);

  // exercise lpc return path
  { 
    auto f = target.lpc([]() -> T { return global; });
    f.wait_reference();
  }
  SHOW("lpc([]&&) -> T", 0, 1, 6);

  { 
    auto f = target.lpc([]() -> T const & { return global; });
    f.wait_reference();
  }
  SHOW("lpc([]&&) -> T const &", 0, 1, 2);

  { 
    T t;
    auto f = target.lpc([&t]() -> T&& { return std::move(t); });
    f.wait_reference();
  }
  SHOW("lpc([]&&) T& -> T&&", 1, 0, 3);

  { 
    T t;
    auto f = target.lpc([&t]() -> T { return t; });
    f.wait_reference();
  }
  SHOW("lpc([]&&) T& -> T", 1, 1, 6);

  { 
    T t;
    auto f = target.lpc([t]() -> T { return t; });
    f.wait_reference();
  }
  SHOW("lpc([]&&) T -> T", 1, 2, 8);

  { 
    T t;
    auto f = target.lpc([&t]() -> T const & { return t; });
    f.wait_reference();
  }
  SHOW("lpc([]&&) T& -> T const &", 1, 1, 2);

  { 
    T t;
    auto f = target.lpc([t]() -> T const & { return t; });
    f.wait_reference();
  }
  SHOW("lpc([]&&) T -> T const &", 1, 2, 4);

  // as_lpc

  using upcxx::operation_cx;
  using upcxx::source_cx;

  upcxx::dist_object<upcxx::global_ptr<int>> dobj(upcxx::new_<int>(0));
  upcxx::global_ptr<int> gp = dobj.fetch(peer).wait();
  int x = 0; int *lp = &x;

  { 
    Fn fn;
    upcxx::rput(42, gp, operation_cx::as_lpc(target, fn));
    while (!done) { upcxx::progress(); }
  }
  done = false;
  SHOW("operation_cx::as_lpc(Fn&) ->", 1, 1, 4);

  { 
    upcxx::rput(42, gp, operation_cx::as_lpc(target, Fn()));
    while (!done) { upcxx::progress(); }
  }
  done = false;
  SHOW("operation_cx::as_lpc(Fn&&) ->", 1, 0, 5);

  { 
    Fn fn;
    upcxx::rput(lp, gp, 1, source_cx::as_lpc(target, fn)|operation_cx::as_future()).wait();
    while (!done) { upcxx::progress(); }
  }
  done = false;
  SHOW("source_cx::as_lpc(Fn&) ->", 1, 1, 6);

  { 
    upcxx::rput(lp, gp, 1, source_cx::as_lpc(target, Fn())|operation_cx::as_future()).wait();
    while (!done) { upcxx::progress(); }
  }
  done = false;
  SHOW("source_cx::as_lpc(Fn&&) ->", 1, 0, 7);

  // then
  using upcxx::future;
  using upcxx::promise;
  using upcxx::make_future;

  { future<T> tf = make_future<T>(global);
  }
  SHOW("make_future<T>", 0, 1, 2);
  future<T> tf = make_future<T>(global);
  T::reset_counts(); // discount construction of tf

  { future<T&> tfr = make_future<T&>(global);
  }
  SHOW("make_future<T&>", 0, 0, 0);
  future<T&> tfr = make_future<T&>(global);

  { promise<T> p;
    future<T> tf = p.get_future();
    p.fulfill_result(global);
    tf.wait_reference();
  }
  SHOW("promise<T>::fulfill_result(T)", 0, 1, 0);

  { promise<T> p;
    future<T> tf = p.get_future();
    p.fulfill_result(T());
    tf.wait_reference();
  }
  SHOW("promise<T>::fulfill_result(T&&)", 1, 0, 1);

  { future<T> f = upcxx::to_future(tf); 
    f.wait_reference();
  }
  SHOW("to_future(future<T>)", 0, 0, 0);

  { future<T> wa = upcxx::when_all(tf); 
    wa.wait_reference();
  }
  SHOW("when_all(future<T>)", 0, 0, 0);

  { future<T,int> wa = upcxx::when_all(tf,4); 
    wa.wait_reference();
  }
  SHOW("when_all(future<T>,int)", 0, 1, 0);

  { future<T&,int> wa = upcxx::when_all(tfr,4); 
    wa.wait_reference();
  }
  SHOW("when_all(future<T&>,int)", 0, 0, 0);

  tf.then([](T t) {}).wait_reference();
  SHOW("future<T>::then T ->", 0, 1, 0);

  tf.then([](T const &t) {}).wait_reference();
  SHOW("future<T>::then T const & ->", 0, 0, 0);

  tf.then([](T t) { return t; }).wait_reference();
  SHOW("future<T>::then T -> T", 0, 1, 4);

  tf.then([](T const &t) { return t; }).wait_reference();
  SHOW("future<T>::then T const & -> T", 0, 1, 3);

  tf.then([](T const &t) -> T const & { return t; }).wait_reference();
  SHOW("future<T>::then T const & -> T const &", 0, 0, 0);

  tfr.then([](T t) {}).wait_reference();
  SHOW("future<T&>::then T ->", 0, 1, 0);

  tfr.then([](T const &t) {}).wait_reference();
  SHOW("future<T&>::then T const & ->", 0, 0, 0);

  tfr.then([](T const &t) -> T const & { return t; }).wait_reference();
  SHOW("future<T&>::then T const & -> T const &", 0, 0, 0);

  print_test_success(success);
  upcxx::finalize();
}
