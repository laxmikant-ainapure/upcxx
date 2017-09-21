int reduce_to_rank0(int my_hits)
{
    // Rank 0 creates an array the size of the number of ranks to store all 
    // the global pointers
    upcxx::global_ptr<int> all_hits_ptr = nullptr;
    if (upcxx::rank_me() == 0) { 
        all_hits_ptr = upcxx::new_array<int>(upcxx::rank_n()); 
    }
    // Rank 0 broadcasts the array global pointer to all ranks
    all_hits_ptr = upcxx::broadcast(all_hits_ptr, 0).wait();
    // All ranks offset the start pointer of the array by their rank to point 
    // to their own chunk of the array
    upcxx::global_ptr<int> my_hits_ptr = all_hits_ptr + upcxx::rank_me();
    // every rank now puts its own hits value into the correct part of the array
    upcxx::rput(my_hits, my_hits_ptr).wait();
    upcxx::barrier();
    // Now rank 0 gets all the values stored in the array
    int hits = 0;
    if (upcxx::rank_me() == 0) {
        // get a local pointer to the shared object on rank 0
        int *local_hits_ptrs = all_hits_ptr.local();
        for (int i = 0; i < upcxx::rank_n(); i++) {
            hits += local_hits_ptrs[i];
        }
        upcxx::delete_array(all_hits_ptr);
    }
    return hits;
}
