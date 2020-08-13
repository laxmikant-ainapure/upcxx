#include <upcxx/upcxx.hpp>

#include "../util.hpp"

struct A {
  int x;
  A(int v) : x(v) {}
  A(const A&) = delete; // non-copyable, non-movable
};

int main() {
  upcxx::init();
  print_test_header();

  upcxx::promise<A> pro1;
  pro1.fulfill_result(3);
  UPCXX_ASSERT_ALWAYS(pro1.get_future().wait_reference().x == 3);

  upcxx::promise<A, A> pro2;
  // undocumented fulfill_result with tuple
  pro2.fulfill_result(std::make_tuple(-1, -2));
  UPCXX_ASSERT_ALWAYS(pro2.get_future().wait_reference<0>().x == -1);
  UPCXX_ASSERT_ALWAYS(pro2.get_future().wait_reference<1>().x == -2);

  print_test_success();
  upcxx::finalize();
}
