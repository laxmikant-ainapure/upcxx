#include <upcxx/upcxx.hpp>
#include <iostream>
#include <memory>

struct A {
  int x;
  A(int v) : x(v) {}
  A(A &&other) : x(other.x){} // move cons
  A(const A &) = delete; // non-copyable
  UPCXX_SERIALIZED_VALUES(x);
};

using namespace upcxx;

int main() {
  upcxx::init();

  // demonstrates that future can contain a non-copyable type (std::unique_ptr)
  future<std::unique_ptr<int>> boof;
  {
    std::unique_ptr<int> up(new int(17));
    boof = make_future(std::move(up)); // trivially ready future
  }
  assert(boof.ready());
  assert(*boof.result_reference() == 17);
  {
    promise<std::unique_ptr<int>> p;
    boof = p.get_future(); // real non-ready future
    assert(!boof.ready());
    std::unique_ptr<int> up(new int(42));
    p.fulfill_result(std::move(up));
  }
  assert(boof.ready());
  assert(*boof.result_reference() == 42);

  // demonstrate with a custom non-copyable type
  future<A> fa;
  { 
    fa = make_future(A(17)); // trivially ready future
  }
  assert(fa.result_reference().x == 17);
  {
    promise<A> p;
    fa = p.get_future(); // real non-ready future
    assert(!fa.ready());
    p.fulfill_result(A(16));
  }
  assert(fa.result_reference().x == 16);

  // demonstrate with a custom non-copyable type and multiple values
  future<A,A> fb;
  { 
    fb = make_future(A(1),A(2)); // trivially ready future
  }
  assert(fb.result_reference<0>().x == 1);
  assert(fb.result_reference<1>().x == 2);
  {
    promise<A,A> p;
    fb = p.get_future(); // real non-ready future
    assert(!fb.ready());
    p.fulfill_result(A(3),A(4));
  }
  assert(fb.result_reference<0>().x == 3);
  assert(fb.result_reference<1>().x == 4);

  { 
    fa = when_all(A(6)); // trivially ready future
  }
  assert(fa.result_reference().x == 6);
  fa = when_all(fa);
  assert(fa.result_reference().x == 6);

  { 
    fb = when_all(A(21),A(22)); // trivially ready future
  }
  assert(fb.result_reference<0>().x == 21);
  assert(fb.result_reference<1>().x == 22);
  fb = when_all(fb);
  assert(fb.result_reference<0>().x == 21);
  assert(fb.result_reference<1>().x == 22);

  upcxx::barrier();
  if (!upcxx::rank_me()) { std::cout << "SUCCESS" << std::endl; }
 
  upcxx::finalize();
  return 0;
}
