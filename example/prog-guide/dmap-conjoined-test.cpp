#include <iostream>
#include <random>
#include "dmap.hpp"

using namespace std;

int main(int argc, char *argv[])
{
  upcxx::init();
  const long N = 100000;
  DistrMap dmap;
  // generators for random keys and values
  mt19937_64 rgen_keys(upcxx::rank_me()), rgen_vals(upcxx::rank_me() + upcxx::rank_n());
  upcxx::future<> fut_all_inserts = upcxx::make_future();
  // insert all key, value pairs into the hash map, wait for operation to complete
  for (long i = 0; i < N; i++) {
    upcxx::future<> fut = dmap.insert(to_string(rgen_keys()), to_string(rgen_vals()));
    fut_all_inserts = upcxx::when_all(fut_all_inserts, fut);
  }
  fut_all_inserts.wait();
  // barrier to ensure all insertions have completed
  upcxx::barrier();
  // now try to fetch keys inserted by neighbor
  int nb = (upcxx::rank_me() + 1) % upcxx::rank_n();
  mt19937_64 rgen_nb_keys(nb), rgen_nb_vals(nb + upcxx::rank_n());
//SNIPPET  
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
//SNIPPET  
  upcxx::finalize();
  return 0;
}


