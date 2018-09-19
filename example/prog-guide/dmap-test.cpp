//SNIPPET
#include <iostream>
#include <random>
#include "dmap.hpp"

using namespace std;

int main(int argc, char *argv[])
{
  upcxx::init();
  const long N = 10000;
  DistrMap dmap;
  // generators for random keys and values
  mt19937_64 rgen_keys(upcxx::rank_me()), rgen_vals(upcxx::rank_me() + upcxx::rank_n());
  // insert all key, value pairs into the hash map, wait for operation to complete
  for (long i = 0; i < N; i++) 
    dmap.insert(to_string(rgen_keys()), to_string(rgen_vals())).wait();
  // barrier to ensure all insertions have completed
  upcxx::barrier();
  // now try to fetch keys inserted by neighbor
  int nb = (upcxx::rank_me() + 1) % upcxx::rank_n();
  mt19937_64 rgen_nb_keys(nb), rgen_nb_vals(nb + upcxx::rank_n());
  // find loop
  for (long i = 0; i < N; i++) {
    string val = dmap.find(to_string(rgen_nb_keys())).wait();
    // check that value is correct
    assert(val == to_string(rgen_nb_vals()));
  }
  upcxx::barrier(); // wait for finds to complete globally
  if (!upcxx::rank_me()) cout << "SUCCESS" << endl;
  upcxx::finalize();
  return 0;
}
//SNIPPET


