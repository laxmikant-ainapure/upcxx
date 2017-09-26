// need to declare a global variable to use with RPC
int hits_counter = 0; 
int hits = 0; 
int reduce_to_rank0(int my_hits)
{
    int expected_hits = upcxx::rank_n();
    // wait for an rpc that updates rank 0's count
    upcxx::rpc(0, [](int my_hits) { hits += my_hits; hits_counter++; }, my_hits).wait();
    // wait until all ranks have updated the count
    if(upcxx::rank_me()==0)
      while( hits_counter < expected_hits ) upcxx::progress();

    // hits is only set for rank 0 at this point, which is ok because only 
    // rank 0 will print out the result
    return hits;
}
