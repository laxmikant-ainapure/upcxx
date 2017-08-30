int hits = 0; 
// counts the number of ranks for which the RPC has completed
int n_done = 0;

int accumulate(int my_hits)
{
    // cannot wait for the RPC - there is no return
    upcxx::rpc_ff(0, [](int my_hits) { hits += my_hits; n_done++; }, my_hits);
    // wait until all ranks have fired off the RPCs
    upcxx::barrier();
    if (upcxx::rank_me() == 0) {
        // spin waiting for all ranks to complete RPCs
        // When spinning, call the progress function to 
        // ensure rank 0 processes waiting RPCs
        while (n_done != upcxx::rank_n()) upcxx::progress();
    }
    return hits;
}
