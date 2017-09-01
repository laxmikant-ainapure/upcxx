#include <upcxx/backend.hpp>
#include <upcxx/allreduce.hpp>
#include <upcxx/broadcast.hpp>
#include <upcxx/wait.hpp>
#include <iostream>

#define KNORM  "\x1B[0m"
#define KLRED "\x1B[91m"

using namespace std;

int main() {
  upcxx::init();

  if (!upcxx::rank_me()) cout << "Testing " << basename(__FILE__) << " with " << upcxx::rank_n() << " ranks" << endl;
  
  int tosend = upcxx::rank_me();

  // broadcast from each rank in turn
  for (int i = 0; i < upcxx::rank_n(); i++) {
      auto fut = upcxx::broadcast(tosend, i);
      int recv = upcxx::wait(fut);
      cout<<"Recv value is "<<recv<<" on "<<upcxx::rank_me()<<"\n";
      upcxx::barrier();
  }
 
  tosend+=42; 
  auto fut2 = upcxx::allreduce(forward<int>(tosend), plus<int>() );
  //auto fut2 = upcxx::allreduce( tosend, std::plus<int>() );
  int recv2 = upcxx::wait(fut2);
  cout<<"Reduced value is "<<recv2<<" on "<<upcxx::rank_me()<<"\n";

  upcxx::finalize();
  return 0;
}
