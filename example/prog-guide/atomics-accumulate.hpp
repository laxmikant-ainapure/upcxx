int accumulate(int my_hits)
{
    // a global point to the atomic counter in rank 0's shared segment
    // Only rank 0 allocates it and then it is broadcast to all other ranks
    upcxx::global_ptr<int32_t> hits_ptr = 
        upcxx::wait(upcxx::broadcast(upcxx::new_<int32_t>(0), 0));
    // now each rank updates the global pointer value using atomics for correctness
    upcxx::wait(upcxx::atomic_fetch_add(hits_ptr, my_hits, memory_order_relaxed));
    // wait until all ranks have updated the counter
    upcxx::barrier();
    // once a global pointer is accessed with atomics, it should always be accessed
    // with atomics in future to prevent unexpected results
    if (upcxx::rank_me() == 0) {
        return upcxx::wait(upcxx::atomic_get(hits_ptr, memory_order_relaxed));
    } else {
        return 0;
    }
}
