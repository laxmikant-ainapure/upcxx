#include <upcxx/backend.hpp>
#include <upcxx/collectives.hpp>
#include <upcxx/wait.hpp>
#include <iostream>

int main() {
  upcxx::init();
  
  std::cout<<"Hello from "<<upcxx::rank_me()<<" of "<<upcxx::rank_n()<<"\n";

  int tosend = upcxx::rank_me();


  auto fut = upcxx::broadcast( tosend, 0 );
  int recv = upcxx::wait(fut);
  std::cout<<"Recv value is "<<recv<<" on "<<upcxx::rank_me()<<"\n";
 
  tosend+=42; 
  auto fut2 = upcxx::allreduce( std::forward<int>(tosend), std::plus<int>() );
  //auto fut2 = upcxx::allreduce( tosend, std::plus<int>() );
  int recv2 = upcxx::wait(fut2);
  std::cout<<"Reduced value is "<<recv2<<" on "<<upcxx::rank_me()<<"\n";

  upcxx::finalize();
  return 0;
}
