#include <upcxx/upcxx.hpp> 


#include <thread>
#include <vector> 

#include <sched.h>

#if !UPCXX_BACKEND_GASNET_PAR
  #error "UPCXX_BACKEND=gasnet_par required."
#endif

using namespace std; 

int main () {
  upcxx::init(); 
  
  //SNIPPET
  const int n = upcxx::rank_n();
  const int me = upcxx::rank_me();

  //create landing zones  

  vector<upcxx::global_ptr<int>> ptrs(n); 
  ptrs[me] = upcxx::new_array<int>(n);

  //share with other processes
  for(int i=0; i < n; i++)
    ptrs[i] = upcxx::broadcast(ptrs[i], i).wait(); 
  
  //initialize local chunk of the landing zone.
  int *local = ptrs[me].local(); 
  for(int i=0; i < n; i++)
    local[i] = -1;

  upcxx::barrier(); 
  
  
  atomic<int> thread_barrier(0);
  int done_count = 0;

  //declare an agreed upon persona for the progress thread
  upcxx::persona progress_persona;

  //create the progress thread
  thread progress_thread( [&]() {
        //capture the progress_persona
        upcxx::persona_scope scope(progress_persona);
        // progress thread drains progress until all work has been performed
        while(done_count != n)
          upcxx::progress();

        cout<<"Progress thread on process "<<upcxx::rank_me()<<" is done"<<endl; 
        //unlock the other threads
        thread_barrier += 1;
      });


  //create another thread to issue the rputs
  thread submit_thread( [&]() {  
      // launch a rput to each rank
      for (int i = 0; i < n ; i++ ) {
        upcxx::rput(
          &me, ptrs[(me + i)%n] + i, 1,
          // each rput is resolved in continuation on the progress thread
          upcxx::operation_cx::as_lpc(
            progress_persona,
            [&,i]() {
              // fetch the value just put
              upcxx::rget(ptrs[(me + i)%n] + i).then(
                [&](int got) {
                  assert(got == me);
                  done_count += 1;
                }
              );
            }
          )
        );
      }
      
      //block here until the progress thread has executed all lpcs
      while(thread_barrier.load(memory_order_acquire) != 1){
        sched_yield();
        upcxx::progress();
      }
    });

  submit_thread.join();
  progress_thread.join();

  upcxx::barrier();

  if ( upcxx::rank_me()==0 )
    cout<<"SUCCESS"<<endl;
  //SNIPPET
    
  upcxx::finalize(); 
}
