int64_t reduce_to_rank0(int64_t my_hits)
{
  upcxx::atomic::domain<int64_t> ad_i64({upcxx::atomic::GET, upcxx::atomic::FADD});

  // a global pointer to the atomic counter in rank 0's shared segment
  upcxx::global_ptr<int64_t> hits_ptr =
      (!upcxx::rank_me() ? upcxx::new_<int64_t>(0) : nullptr);
  // rank 0 allocates and then broadcasts the global pointer to all other ranks
  hits_ptr = upcxx::broadcast(hits_ptr, 0).wait();
  // now each rank updates the global pointer value using atomics for correctness
  ad_i64.fadd(hits_ptr, memory_order_relaxed, my_hits).wait();
  // wait until all ranks have updated the counter
  upcxx::barrier();
  // once a memory location is accessed with atomics, it should only be
  // subsequently accessed using atomics to prevent unexpected results
  if (upcxx::rank_me() == 0) {
    return ad_i64.get(hits_ptr, memory_order_relaxed).wait();
  } else {
    return 0;
  }
}
