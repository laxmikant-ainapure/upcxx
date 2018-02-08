#include <chrono>
#include <upcxx/backend.hpp>
#include <upcxx/allocate.hpp>
#include <upcxx/global_ptr.hpp>
#include <upcxx/rpc.hpp>
#include <upcxx/future.hpp>
#include <upcxx/atomic.hpp>
#include <upcxx/allreduce.hpp>

#include "util.hpp"

using upcxx::rank_me;
using upcxx::rank_n;
using upcxx::barrier;
using upcxx::global_ptr;

namespace chrono = std::chrono;
using std::memory_order_relaxed;

template<typename T> 
struct max_op : std::binary_function<T, T, T>
{
    T operator() (const T& x, const T& y) const {return std::max(x, y);}
};

chrono::time_point<chrono::high_resolution_clock> timer_start(void)
{
    return chrono::high_resolution_clock::now();
}

double timer_elapsed(chrono::time_point<chrono::high_resolution_clock> t)
{
    chrono::duration<double> elapsed_t = chrono::high_resolution_clock::now() - t;
    return elapsed_t.count();
}

global_ptr<int64_t> counter;
global_ptr<int64_t> target_counter;

const int iters = 250000;
const int warm_up_iters = 10000;
  
int main(int argc, char **argv)
{
  upcxx::init();
  
  upcxx::atomic::domain<int64_t> ad_i64({upcxx::atomic::GET, upcxx::atomic::SET, 
          upcxx::atomic::FADD});
  
  print_test_header();

  if (rank_me() == 0) {
    counter = upcxx::allocate<int64_t>();
    ad_i64.set(counter, memory_order_relaxed, (int64_t)0).wait();
  }
  
  barrier();
  
  // get the global pointer to the target counter
  target_counter = upcxx::rpc(0, []() { return counter; }).wait();
  
  for (int i = 0; i < warm_up_iters; i++) {
    ad_i64.fadd(target_counter, memory_order_relaxed, (int64_t)1).wait();
  }
  
  upcxx::barrier();
  auto t = timer_start();
  upcxx::future<> f_chain = upcxx::make_future();
  for (int i = 0; i < iters; i++) {
    auto f = ad_i64.fadd(target_counter, memory_order_relaxed, (int64_t)1).then([](int64_t op1){;});
    //f.wait();
    f_chain = upcxx::when_all(f_chain, f);
    if (i % 10 == 0) upcxx::progress();
  }
  f_chain.wait();
  auto t_used = timer_elapsed(t);
  upcxx::barrier();
  
  double t_max_used = upcxx::allreduce(t_used, max_op<double>()).wait();
  double t_av_used = upcxx::allreduce(t_used, std::plus<double>()).wait() / upcxx::rank_n();
  if (upcxx::rank_me() == 0) 
    printf("Time taken: %.3f s (max), %.3f s (avg)\n", t_max_used, t_av_used);
  upcxx::barrier();  
  upcxx::finalize();
  return 0;
}
