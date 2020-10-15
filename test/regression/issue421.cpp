#include <upcxx/upcxx.hpp>
#include <iostream>
#include <assert.h>

using namespace upcxx;

int main() {
  upcxx::init();

  global_ptr<int> gp1 = new_<int>();
  global_ptr<int> gp2 = new_<int>();
  *gp1.local() = 42;
  static bool done = false;
  copy(gp1, gp2, 1, remote_cx::as_rpc([]() { done = true; }));
  while (!done) upcxx::progress();
  assert(*gp2.local() == 42);
 
  barrier();
  delete_(gp1);
  delete_(gp2);
  if (!rank_me()) std::cout << "SUCCESS" << std::endl;
  upcxx::finalize();
  return 0;
}
