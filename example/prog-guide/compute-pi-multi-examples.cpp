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

namespace upcxx {
    // fill in missing functions
    template<typename T>
    //upcxx::future<T> broadcast(T &&value, upcxx::intrank_t sender, upcxx::team &team = world ()) {
    upcxx::future<T> broadcast(T &&value, upcxx::intrank_t sender) {
        if (upcxx::rank_me() == 0) cerr << "upcxx::broadcast not yet implemented" << endl;
        upcxx::finalize();
        exit(1);
    }
}

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
    hits = version::accumulate(my_hits);                                       \
    if (upcxx::rank_me() == 0) {                                        \
        cout << #version << ": pi estimate: " << 4.0 * hits / trials   \
             << ", rank 0 alone: " << 4.0 * my_hits / my_trials << endl; \
    }


int main(int argc, char **argv)
{
    upcxx::init();
    int my_hits = 0, trials = 100000;
    if (argc == 2) trials = atoi(argv[1]);
    int my_trials = (trials + upcxx::rank_n() - 1) / upcxx::rank_n();
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
