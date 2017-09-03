int accumulate(int my_hits)
{
    // a global pointer to the atomic counter in rank 0's shared segment
    upcxx::global_ptr<int32_t> hits_ptr = 
        (!upcxx::rank_me() ? upcxx::new_<int32_t>(0) : nullptr);
    // rank 0 allocates and then broadcasts the global pointer to all other ranks
    hits_ptr = upcxx::wait(upcxx::broadcast(hits_ptr, 0));
    // now each rank updates the global pointer value using atomics for correctness
    upcxx::wait(upcxx::atomic_fetch_add(hits_ptr, my_hits, memory_order_relaxed));
    // wait until all ranks have updated the counter
    upcxx::barrier();
    // once a memory location is accessed with atomics, it should only be   
    // subsequently accessed using atomics to prevent unexpected results
    if (upcxx::rank_me() == 0) {
        return upcxx::wait(upcxx::atomic_get(hits_ptr, memory_order_relaxed));
    } else {
        return 0;
    }
}
