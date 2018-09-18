#include <iostream>
#include <random>
#include <memory>
#include "dmap-ff.hpp"

using namespace std;

int main(int argc, char *argv[])
{
  upcxx::init();
  const long N = 100000;
  DistrMap dmap;
  // generators for random keys and values
  mt19937_64 rgen_keys(upcxx::rank_me()), rgen_vals(upcxx::rank_me() + upcxx::rank_n());
//SNIPPET
  // keep track of how many inserts have been made to each target process
  std::unique_ptr<int64_t[]> inserts_per_rank(new int64_t[upcxx::rank_n()]);
  // insert all key-value pairs into the hash map
  for (long i = 0; i < N; i++) {
    auto key = to_string(rgen_keys());
    dmap.insert(key, to_string(rgen_vals()));
    inserts_per_rank[dmap.get_target_rank(key)]++;
  }
  // setup atomic domain with only the operations needed
  upcxx::atomic_domain<int64_t> ad({upcxx::atomic_op::load, upcxx::atomic_op::add});
  // distributed object to keep track of number of inserts expected at every process
  upcxx::dist_object<upcxx::global_ptr<int64_t> > n_inserts(upcxx::new_<int64_t>(0));
  // get pointers for all other processes and use atomics to update remote counters
  for (long i = 0; i < upcxx::rank_n(); i++) {
    if (inserts_per_rank) {
      auto remote_n_inserts = n_inserts.fetch(i).wait();
      // use atomics to increment the remote process's expected count of inserts
      ad.add(remote_n_inserts, inserts_per_rank[i], memory_order_relaxed).wait();
    }
  }
  upcxx::barrier();
  // wait until we have received all the expected updates, spinning on progress
  int64_t curr_inserts = 0;
  do {
    // Note: once a memory location is accessed with atomics, it should only be
    // subsequently accessed using atomics to prevent unexpected results
    curr_inserts = ad.load(*n_inserts, memory_order_relaxed).wait();
    upcxx::progress();
  } while (dmap.local_size() != curr_inserts);
//SNIPPET  
  // now try to fetch keys inserted by neighbor
  int nb = (upcxx::rank_me() + 1) % upcxx::rank_n();
  mt19937_64 rgen_nb_keys(nb), rgen_nb_vals(nb + upcxx::rank_n());
  // the start of the conjoined futures
  upcxx::future<> fut_all = upcxx::make_future();
  for (long i = 0; i < N; i++) {
    auto key = rgen_nb_keys();
    auto expected_val = rgen_nb_vals();
    // attach callback, which itself returns a future 
    upcxx::future<> fut = dmap.find(to_string(key)).then(
      // lambda to check the return value
      [expected_val](string val) {
        assert(val == to_string(expected_val));
      });
    // conjoin the futures
    fut_all = upcxx::when_all(fut_all, fut);
  }
  // wait for all the conjoined futures to complete
  fut_all.wait();
  upcxx::finalize();
  return 0;
}


