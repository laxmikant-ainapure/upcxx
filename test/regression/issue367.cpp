#include <upcxx/upcxx.hpp>
#include <iostream>
#include <assert.h>

using namespace upcxx;

std::vector<int> myvec{1,2,3}; // global variable
const int &getter(size_t idx) { return myvec[idx]; }
int main() {
  upcxx::init();
  int peer = (rank_me()+1)%rank_n();

  std::vector<int> v1 = rpc(peer, /*UserFn=*/[](){ return std::vector<int>{1,2,3}; }).wait();
  assert(v1[2] == 3);
  std::vector<int> v2 = rpc(peer, /*UserFn=*/[](){ return myvec; }).wait();
  assert(v2[2] == 3);
  std::vector<int> v3 = rpc(peer, /*UserFn=*/[]() -> std::vector<int> const { return myvec; }).wait();
  assert(v3[2] == 3);
  #ifndef WORKAROUND
  std::vector<int> v4 = rpc(peer, /*UserFn=*/[]() -> std::vector<int> const & { return myvec; }).wait();
  assert(v4[2] == 3);
  int v5 = rpc(0, /*UserFn=*/getter, 2).wait();
  assert(v5 == 3);
  barrier();
  std::vector<int> v6 = rpc(peer, /*UserFn=*/[]() -> std::vector<int>&& { return std::move(myvec); }).wait();
  assert(v6[2] == 3);
  #endif

  barrier();
  if (!rank_me()) std::cout << "SUCCESS" << std::endl;
  upcxx::finalize();
  return 0;
}
