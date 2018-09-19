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
  auto next_key = to_string(rgen_keys());
  auto next_val = to_string(rgen_vals());
  // insert all key, value pairs into the hash map, wait for operation to complete
  for (long i = 0; i < N; i++) {
    upcxx::future<> fut = dmap.insert(next_key, next_val);
    // perform computation while waiting for RPC to complete
    if (i < N - 1) {
      next_key = to_string(rgen_keys());
      next_val = to_string(rgen_vals());
    }
    // wait for operation to complete before next insert
    fut.wait();
  }
  // barrier to ensure all insertions have completed
  upcxx::barrier();
  // now try to fetch keys inserted by neighbor
  int nb = (upcxx::rank_me() + 1) % upcxx::rank_n();
  mt19937_64 rgen_nb_keys(nb), rgen_nb_vals(nb + upcxx::rank_n());
//SNIPPET
  for (long i = 0; i < N; i++) {
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
//SNIPPET
  upcxx::barrier(); // wait for finds to complete globally
  if (!upcxx::rank_me()) cout << "SUCCESS" << endl;
  upcxx::finalize();
  return 0;
}


