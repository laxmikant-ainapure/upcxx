#include <upcxx/upcxx.hpp>
#include "util.hpp"

struct T {
  int value;
  bool valid = true;
  T() {}
  T(int v) : value(v) {}
  T(T const &that) : value(that.value) {
    UPCXX_ASSERT_ALWAYS(that.valid, "copying from an invalidated object");
  }
  T(T &&that) : value(that.value) {
    UPCXX_ASSERT_ALWAYS(that.valid, "moving from an invalidated object");
    that.valid = false;
    that.value = -1000;
  }
  ~T() {
    valid = false;
    value = -1000;
  }
  UPCXX_SERIALIZED_FIELDS(value, valid)
};

bool done = false;

struct Fn {
  T t;
  Fn() {}
  Fn(int v) : t(v) {}
  T operator()() {
    UPCXX_ASSERT_ALWAYS(t.valid);
    UPCXX_ASSERT_ALWAYS(t.value == upcxx::rank_me() + 200);
    done = true;
    return T(t.value + 1);
  }
  UPCXX_SERIALIZED_FIELDS(t)
};

int main() {
  upcxx::init();
  print_test_header();

  int target = (upcxx::rank_me() + 1) % upcxx::rank_n();

  {
    // rvalue, default completions
    auto fut = upcxx::rpc(target,
                 [](T &&x) {
                   UPCXX_ASSERT_ALWAYS(x.valid);
                   UPCXX_ASSERT_ALWAYS(x.value == 10);
                   return T(x.value + 1);
                 },
                 T(10)
               );
    UPCXX_ASSERT_ALWAYS(fut.wait_reference().valid);
    UPCXX_ASSERT_ALWAYS(fut.wait_reference().value == 11);
  }

  {
    // lvalue, default completions
    T *tmp = new T(target + 100);
    auto fut = upcxx::rpc(target,
                 [](const T &x) -> const T& {
                   UPCXX_ASSERT_ALWAYS(x.valid);
                   UPCXX_ASSERT_ALWAYS(x.value == upcxx::rank_me() + 100);
                   return x;
                 },
                 *tmp
               );
    delete tmp;
    UPCXX_ASSERT_ALWAYS(fut.wait_reference().valid);
    UPCXX_ASSERT_ALWAYS(fut.wait_reference().value == target + 100);
  }

  {
    // view, default completions
    T *tmp = new T(target + 100);
    auto fut = upcxx::rpc(target,
                 [](upcxx::view<T> x) {
                   UPCXX_ASSERT_ALWAYS((*x.begin()).valid);
                   UPCXX_ASSERT_ALWAYS((*x.begin()).value == upcxx::rank_me() + 100);
                   return *x.begin();
                 },
                 upcxx::make_view(tmp, tmp + 1)
               );
    delete tmp;
    UPCXX_ASSERT_ALWAYS(fut.wait_reference().valid);
    UPCXX_ASSERT_ALWAYS(fut.wait_reference().value == target + 100);
  }

  {
    // function object rvalue, default completions
    auto fut = upcxx::rpc(target, Fn(target + 200));
    UPCXX_ASSERT_ALWAYS(fut.wait_reference().valid);
    UPCXX_ASSERT_ALWAYS(fut.wait_reference().value == target + 201);
  }

  {
    // function object lvalue, default completions
    Fn *tmp = new Fn(target + 200);
    auto fut = upcxx::rpc(target, *tmp);
    delete tmp;
    UPCXX_ASSERT_ALWAYS(fut.wait_reference().valid);
    UPCXX_ASSERT_ALWAYS(fut.wait_reference().value == target + 201);
  }

  // reset state for rpc_ff
  upcxx::barrier();
  done = false;
  upcxx::barrier();

  {
    // rpc_ff, rvalue, default completions
    upcxx::rpc_ff(target,
      [](T &&x) {
        UPCXX_ASSERT_ALWAYS(x.valid);
        UPCXX_ASSERT_ALWAYS(x.value == upcxx::rank_me() + 400);
        done = true;
      },
      T(target + 400)
    );
    while (!done) { upcxx::progress(); }
    done = false;
    upcxx::barrier();
  }

  {
    // rpc_ff, lvalue, default completions
    T *tmp = new T(target + 100);
    upcxx::rpc_ff(target,
      [](const T &x) {
        UPCXX_ASSERT_ALWAYS(x.valid);
        UPCXX_ASSERT_ALWAYS(x.value == upcxx::rank_me() + 100);
        done = true;
      },
      *tmp
    );
    delete tmp;
    while (!done) { upcxx::progress(); }
    done = false;
    upcxx::barrier();
  }

  {
    // rpc_ff, view, default completions
    T *tmp = new T(target + 100);
    upcxx::rpc_ff(target,
      [](upcxx::view<T> x) {
        UPCXX_ASSERT_ALWAYS((*x.begin()).valid);
        UPCXX_ASSERT_ALWAYS((*x.begin()).value == upcxx::rank_me() + 100);
        done = true;
      },
      upcxx::make_view(tmp, tmp + 1)
    );
    delete tmp;
    while (!done) { upcxx::progress(); }
    done = false;
    upcxx::barrier();
  }

  {
    // rpc_ff, rvalue function object, default completions
    upcxx::rpc_ff(target, Fn(target + 200));
    while (!done) { upcxx::progress(); }
    done = false;
    upcxx::barrier();
  }

  {
    // rpc_ff, lvalue function object, default completions
    Fn *tmp = new Fn(target + 200);
    upcxx::rpc_ff(target, *tmp);
    delete tmp;
    while (!done) { upcxx::progress(); }
    done = false;
    upcxx::barrier();
  }

  //////////////////////////////////////////////////////////////////////////////

  auto sc_cx = upcxx::source_cx::as_future();
  auto sc_op_cx = sc_cx | upcxx::operation_cx::as_future();

  {
    // rvalue, source_cx::as_future
    auto futs = upcxx::rpc(target,
                  sc_op_cx,
                  [](T &&x) {
                    UPCXX_ASSERT_ALWAYS(x.valid);
                    UPCXX_ASSERT_ALWAYS(x.value == 10);
                    return T(x.value + 1);
                  },
                  T(10)
                );
    UPCXX_ASSERT_ALWAYS(std::get<1>(futs).wait_reference().valid);
    UPCXX_ASSERT_ALWAYS(std::get<1>(futs).wait_reference().value == 11);
    std::get<0>(futs).wait();
  }

  {
    // lvalue, source_cx::as_future
    T *tmp = new T(target + 100);
    auto futs = upcxx::rpc(target,
                  sc_op_cx,
                  [](const T &x) -> const T& {
                    UPCXX_ASSERT_ALWAYS(x.valid);
                    UPCXX_ASSERT_ALWAYS(x.value == upcxx::rank_me() + 100);
                    return x;
                  },
                  *tmp
                );
    std::get<0>(futs).wait();
    delete tmp;
    UPCXX_ASSERT_ALWAYS(std::get<1>(futs).wait_reference().valid);
    UPCXX_ASSERT_ALWAYS(std::get<1>(futs).wait_reference().value == target + 100);
  }

  {
    // view, source_cx::as_future
    T *tmp = new T(target + 100);
    auto futs = upcxx::rpc(target,
                  sc_op_cx,
                  [](upcxx::view<T> x) {
                    UPCXX_ASSERT_ALWAYS((*x.begin()).valid);
                    UPCXX_ASSERT_ALWAYS((*x.begin()).value == upcxx::rank_me() + 100);
                    return *x.begin();
                  },
                  upcxx::make_view(tmp, tmp + 1)
                );
    std::get<0>(futs).wait();
    delete tmp;
    UPCXX_ASSERT_ALWAYS(std::get<1>(futs).wait_reference().valid);
    UPCXX_ASSERT_ALWAYS(std::get<1>(futs).wait_reference().value == target + 100);
  }

  {
    // function object rvalue, source_cx::as_future
    auto futs = upcxx::rpc(target, sc_op_cx, Fn(target + 200));
    UPCXX_ASSERT_ALWAYS(std::get<1>(futs).wait_reference().valid);
    UPCXX_ASSERT_ALWAYS(std::get<1>(futs).wait_reference().value == target + 201);
    std::get<0>(futs).wait();
  }

  {
    // function object lvalue, source_cx::as_future
    Fn *tmp = new Fn(target + 200);
    auto futs = upcxx::rpc(target, sc_op_cx, *tmp);
    std::get<0>(futs).wait();
    delete tmp;
    UPCXX_ASSERT_ALWAYS(std::get<1>(futs).wait_reference().valid);
    UPCXX_ASSERT_ALWAYS(std::get<1>(futs).wait_reference().value == target + 201);
  }

  // reset state for rpc_ff
  upcxx::barrier();
  done = false;
  upcxx::barrier();

  {
    // rpc_ff, rvalue, source_cx::as_future
    auto fut = upcxx::rpc_ff(target,
                 sc_cx,
                 [](T &&x) {
                   UPCXX_ASSERT_ALWAYS(x.valid);
                   UPCXX_ASSERT_ALWAYS(x.value == upcxx::rank_me() + 400);
                   done = true;
                 },
                 T(target + 400)
               );
    while (!done) { upcxx::progress(); }
    fut.wait();
    done = false;
    upcxx::barrier();
  }

  {
    // rpc_ff, lvalue, source_cx::as_future
    T *tmp = new T(target + 100);
    auto fut = upcxx::rpc_ff(target,
                 sc_cx,
                 [](const T &x) {
                   UPCXX_ASSERT_ALWAYS(x.valid);
                   UPCXX_ASSERT_ALWAYS(x.value == upcxx::rank_me() + 100);
                   done = true;
                 },
                 *tmp
               );
    fut.wait();
    delete tmp;
    while (!done) { upcxx::progress(); }
    done = false;
    upcxx::barrier();
  }

  {
    // rpc_ff, view, source_cx::as_future
    T *tmp = new T(target + 100);
    auto fut = upcxx::rpc_ff(target,
                 sc_cx,
                 [](upcxx::view<T> x) {
                   UPCXX_ASSERT_ALWAYS((*x.begin()).valid);
                   UPCXX_ASSERT_ALWAYS((*x.begin()).value == upcxx::rank_me() + 100);
                   done = true;
                 },
                 upcxx::make_view(tmp, tmp + 1)
               );
    fut.wait();
    delete tmp;
    while (!done) { upcxx::progress(); }
    done = false;
    upcxx::barrier();
  }

  {
    // rpc_ff, rvalue function object, source_cx::as_future
    auto fut = upcxx::rpc_ff(target, sc_cx, Fn(target + 200));
    while (!done) { upcxx::progress(); }
    fut.wait();
    done = false;
    upcxx::barrier();
  }

  {
    // rpc_ff, lvalue function object, source_cx::as_future
    Fn *tmp = new Fn(target + 200);
    auto fut = upcxx::rpc_ff(target, sc_cx, *tmp);
    fut.wait();
    delete tmp;
    while (!done) { upcxx::progress(); }
    done = false;
    upcxx::barrier();
  }

  print_test_success();
  upcxx::finalize();
}
