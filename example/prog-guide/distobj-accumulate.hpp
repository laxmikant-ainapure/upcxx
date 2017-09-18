int accumulate(int my_hits)
{
    // declare a distributed on every rank
    upcxx::dist_object<int> all_hits(0);
    // set the local value of the distributed object on each rank
    *all_hits = my_hits;
    upcxx::barrier();
    int hits = 0;
    if (upcxx::rank_me() == 0) {
        // rank 0 accumulates the values
        for (int i = 0; i < upcxx::rank_n(); i++) {
            // fetch the distributed object from remote rank i
            hits += fetch(all_hits, i).wait();
        }
    }
    // ensure that no distributed objects are destructed before rank 0 is done
    upcxx::barrier();
    return hits;
}
