// need to declare a global variable to use with RPC
int hits = 0; 
int accumulate(int my_hits)
{
    // wait for an rpc that updates rank 0's count
    upcxx::wait(upcxx::rpc(0, [](int my_hits) { hits += my_hits; }, my_hits));
    // wait until all ranks have updated the count
    upcxx::barrier();
    // hits is only set for rank 0 at this point, which is ok because only 
    // rank 0 will print out the result
    return hits;
}
