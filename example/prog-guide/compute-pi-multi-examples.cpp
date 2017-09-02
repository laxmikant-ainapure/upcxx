/*
 * This is the main file for all the compute pi code in the guide. It wraps all of the various
 * allocate() calls, using namespaces. Although this is a rather peculiar structure, it is intended
 * for use with individual code snippets extracted directly from the programmer's guide.
*/


#include <iostream>
#include <cstdlib>
#include <random>
#include <upcxx/upcxx.hpp>

using namespace std;

#include "fetch.hpp"

namespace rpc {
    #include "rpc-accumulate.hpp"
}

namespace global_ptrs {
    #include "global-ptrs-accumulate.hpp"
}

namespace distobj {
    #include "distobj-accumulate.hpp"
}

namespace async_distobj {
    #include "async-distobj-accumulate.hpp"
}

namespace atomics {
    #include "atomics-accumulate.hpp"
}

namespace quiesence {
    #include "quiesence-accumulate.hpp"
}

int hit()
{
    double x = static_cast<double>(rand()) / RAND_MAX;
    double y = static_cast<double>(rand()) / RAND_MAX;
    if (x*x + y*y <= 1.0) return 1;
    else return 0;
}


#define ACCM(version)                                                   \
    hits = version::accumulate(my_hits);                                \
	if (upcxx::rank_me() == 0) {										\
        cout << #version << ": pi estimate: " << 4.0 * hits / trials    \
             << ", rank 0 alone: " << 4.0 * my_hits / my_trials << endl; \
	}



int main(int argc, char **argv)
{
    upcxx::init();
    int my_hits = 0;
    int my_trials = 100000;
    if (argc >= 2) my_trials = atoi(argv[1]);
    int trials = upcxx::rank_n() * my_trials;
    if (!upcxx::rank_me()) 
      cout << "Calculating pi with " << trials << " trials, distributed across " << upcxx::rank_n() << " ranks." << endl;
    srand(upcxx::rank_me());
    for (int i = 0; i < my_trials; i++) {
        my_hits += hit();
    }

    int hits;
    
    ACCM(rpc);
    ACCM(global_ptrs);
    ACCM(distobj);
    ACCM(async_distobj);
    ACCM(atomics);
    ACCM(quiesence);

    upcxx::finalize();
    return 0;
}
