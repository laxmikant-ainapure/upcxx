#include <mutex>
#include <list>
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

int done = 0;

int main(int argc, char **argv)
{
    upcxx::init();
    srand(upcxx::rank_me());

    if (upcxx::rank_me() == 0) {
        // the number of trials to run on each rank
        int trials_per_rank = 100000;
        if (argc == 2) trials_per_rank = atoi(argv[1]);

        int hits = 0;
        int my_hits = 0;

        upcxx::persona scheduler_persona;
        mutex scheduler_lock;
        list<upcxx::future<int> > remote_rpcs;
        {
            // Scope block delimits domain of persona scope instance
            auto scope = upcxx::persona_scope(scheduler_lock, scheduler_persona);
            // All following upcxx actions will use scheduler_persona as current
            for (int rank = 1; rank < upcxx::rank_n(); rank++) {
                // launch computations on remote ranks and store the
                // returned future in the list of remote rpcs
                remote_rpcs.push_back( 
                    upcxx::rpc(rank,
                               [](int my_trials) {
                                   int my_hits = 0;
                                   for (int i = 0; i < my_trials; i++) {
                                       my_hits += hit();
                                   }
                                   done = 1;
                                   return my_hits;
                               },
                               trials_per_rank));
            }
            // scope destructs :
            // - scheduler_persona dropped from active set if it
            //   wasn't active before the scope's construction
            // - Previously current persona revived
            // - Lock released
        }
        #pragma omp parallel sections default(shared)
        {
            // This is the computational thread of rank 0
            #pragma omp section
            {
                // do the computation
                for (int i = 0; i < trials_per_rank; i++) {
                    my_hits += hit();
                }
            } // end omp section
            // Launch another thread to make progress and track completion
            // of the operations
            #pragma omp section
            {
                auto scope = upcxx::persona_scope(scheduler_lock, scheduler_persona);
                while (!remote_rpcs.empty()) {
                    upcxx::progress();
                    auto it = find_if(remote_rpcs.begin(), remote_rpcs.end(),
                                      [](upcxx::future<int> & f) {return f.ready();});
                    if (it != remote_rpcs.end()) {
                        auto &fut = *it;
                        // accumulate the result
                        hits += fut.result();
                        // remove the future from the list
                        remote_rpcs.erase(it);
                    }
                } 
            } // end omp section
        } // end omp parallel sections
        hits += my_hits;
        // the total number of trials over all ranks
        int trials = upcxx::rank_n() * trials_per_rank;
        cout << "pi estimated as " << 4.0 * hits / trials << endl;
    } else {
        // other ranks progress until quiescence is reached (i.e. done == 1)
        while (!done) upcxx::progress();
    }
    upcxx::finalize();
    return 0;
}
