#include <iostream>
#include <cstdlib>
#include <random>
#include <upcxx/upcxx.hpp>

using namespace std;

// choose a point at random
int hit()
{
    double x = static_cast<double>(rand()) / RAND_MAX;
    double y = static_cast<double>(rand()) / RAND_MAX;
    if (x*x + y*y <= 1.0) return 1;
    else return 0;
}

// accumulate the hits 
int accumulate(int my_hits)
{
    // wait for a collective reduction that sums all local values
    return upcxx::wait(upcxx::allreduce( std::forward<int>(my_hits), plus<int>() ));
}

int main(int argc, char **argv)
{
    upcxx::init();
    // each rank gets its own copy of local variables
    int my_hits = 0;
		// keep the number of trials per rank low to show the difference between single and multiple ranks
    int my_trials = 2;
    // each rank gets its own local copies of input arguments
    if (argc == 2) my_trials = atoi(argv[1]);
    // initialize the random number generator differently for each rank
    srand(upcxx::rank_me());
    // do the computation
    for (int i = 0; i < my_trials; i++) {
        my_hits += hit();
    }
    // accumulate and print out the final result
    int hits = accumulate(my_hits);
    // only rank 0 prints the result
    if (upcxx::rank_me() == 0) {
			  // the total number of trials over all ranks
		    int trials = upcxx::rank_n() * my_trials;
			  cout << "trials " << trials << " my_trials " << my_trials << endl;
        cout << "pi estimate: " << 4.0 * hits / trials << endl;
    }
    upcxx::finalize();
    return 0;
}
