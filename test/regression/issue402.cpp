#include <upcxx/upcxx.hpp>

#include "../util.hpp"

struct B {
  int x;
  B(int v) : x(v) {}
  B(const B&) = delete; // non-copyable, non-movable
};

struct A {
  int x;
  A(int v) : x(v) {}
  A(const B& b) : x(b.x) {}
  A(const A&) = delete; // non-copyable, non-movable
};

int main() {
  upcxx::init();
  print_test_header();

  {
    upcxx::promise<A> pro;
    pro.fulfill_result(3);
    UPCXX_ASSERT_ALWAYS(pro.get_future().wait_reference().x == 3);
  }
  {
    upcxx::promise<A, A> pro;
    pro.fulfill_result(4, 5);
    UPCXX_ASSERT_ALWAYS(pro.get_future().wait_reference<0>().x == 4);
    UPCXX_ASSERT_ALWAYS(pro.get_future().wait_reference<1>().x == 5);
  }

  // undocumented fulfill_result with tuple
  {
    upcxx::promise<A> pro;
    pro.fulfill_result(std::make_tuple(-3));
    UPCXX_ASSERT_ALWAYS(pro.get_future().wait_reference().x == -3);
  }
  {
    upcxx::promise<A, A> pro;
    pro.fulfill_result(std::make_tuple(-1, -2));
    UPCXX_ASSERT_ALWAYS(pro.get_future().wait_reference<0>().x == -1);
    UPCXX_ASSERT_ALWAYS(pro.get_future().wait_reference<1>().x == -2);
  }

  // construct non-movable A from non-movable B
  {
    upcxx::promise<A> pro;
    pro.fulfill_result(B(3));
    UPCXX_ASSERT_ALWAYS(pro.get_future().wait_reference().x == 3);
  }
  {
    upcxx::promise<A, A> pro;
    pro.fulfill_result(B(4), B(5));
    UPCXX_ASSERT_ALWAYS(pro.get_future().wait_reference<0>().x == 4);
    UPCXX_ASSERT_ALWAYS(pro.get_future().wait_reference<1>().x == 5);
  }

  print_test_success();
  upcxx::finalize();
}
