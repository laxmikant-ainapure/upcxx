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
  // distributed object to keep track of number of inserts expected at this process
  upcxx::dist_object<long> n_inserts = 0;
  // keep track of how many inserts have been made to each target process
  std::unique_ptr<long[]> inserts_per_rank(new long[upcxx::rank_n()]);
  // insert all key-value pairs into the hash map
  for (long i = 0; i < N; i++) {
    auto key = to_string(rgen_keys());
    // insert has no return because it uses rpc_ff
    dmap.insert(key, to_string(rgen_vals()));
    // increment the count for the target process
    inserts_per_rank[dmap.get_target_rank(key)]++;
  }
  // update all remote processes with the expected count
  for (long i = 0; i < upcxx::rank_n(); i++) {
    if (inserts_per_rank[i]) {
      // use rpc to update the remote process's expected count of inserts
      upcxx::rpc(i,
                 [](upcxx::dist_object<long> &e_inserts, long count) {
                   *e_inserts += count;
                 }, n_inserts, inserts_per_rank[i]).wait();
    }
  }
  // wait until all threads have updated insert counts
  upcxx::barrier();
  // wait until we have received all the expected updates, spinning on progress
  while (dmap.local_size() < *n_inserts) upcxx::progress();
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


