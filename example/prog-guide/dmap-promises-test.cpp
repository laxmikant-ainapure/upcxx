#include <iostream>
#include <random>
#include "dmap-promises.hpp"
#include "timers.hpp"

using namespace std;

int main(int argc, char *argv[])
{
  upcxx::init();
  const long N = 100000;
  DistrMap dmap;
  // generators for random keys and values
  mt19937_64 rgen_keys(upcxx::rank_me()), rgen_vals(upcxx::rank_me() + upcxx::rank_n());
  auto t = timer_start();
//SNIPPET
  // create an empty promise, to be used for tracking operations
  upcxx::promise<> prom;
  // insert all key, value pairs into the hash map
  for (long i = 0; i < N; i++) 
    dmap.insert(to_string(rgen_keys()), to_string(rgen_vals()), prom);
  // finalize the promise
  upcxx::future<> fut = prom.finalize();
  // wait for the operations to complete
  fut.wait();
//SNIPPET  
  // barrier to ensure all insertions have completed
  upcxx::barrier();
  if (!upcxx::rank_me()) fprintf(stderr, "inserts took %.2f s\n", timer_elapsed(t));
  t = timer_start();
  // now try to fetch keys inserted by neighbor
  int nb = (upcxx::rank_me() + 1) % upcxx::rank_n();
  mt19937_64 rgen_nb_keys(nb), rgen_nb_vals(nb + upcxx::rank_n());
  upcxx::future<> fut_all = upcxx::make_future();
  for (long i = 0; i < N; i++) {
    // conjoin the futures
    fut_all = upcxx::when_all(fut_all, fut);
    auto key = rgen_nb_keys();
    auto expected_val = rgen_nb_vals();
    // attach callback, which itself returns a future 
    upcxx::future<> fut = dmap.find(to_string(key)).then(
      // lambda to check the return value
      [expected_val](string val) {
        assert(val == to_string(expected_val));
      });
    // wait for future and its callback to complete
    fut.wait();
  }
  // wait for all the conjoined futures to complete
  fut_all.wait();
  upcxx::barrier();
  if (!upcxx::rank_me()) fprintf(stderr, "finds took %.2f s\n", timer_elapsed(t));
  upcxx::finalize();
  return 0;
}


