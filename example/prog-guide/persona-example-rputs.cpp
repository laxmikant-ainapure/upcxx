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
	
	
	int tn = 10;
	vector<thread*> ts(tn);
  int done_count = 0;

  //declare an agreed upon persona for the progress thread
  upcxx::persona progress_persona;

  thread progress_thread( [&]() {
        //capture the progress_persona
        upcxx::persona_scope scope(progress_persona);
		    // progress thread drains progress until all work has been performed
		    while(done_count != n*tn)
			    upcxx::progress();
      });

 

	for(int tid=0; tid < tn; tid++) {
		ts[tid] = new thread([&,tid]() {
	  	// each thread launches n rputs to each rank
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
	  	
      upcxx::discharge();
	  });
  }

	for(int tid=1; tid < tn; tid++) {
		ts[tid]->join();
		delete ts[tid];
	}
  progress_thread.join();

	upcxx::barrier();

  if ( upcxx::rank_me()==0 )
    cout<<"SUCCESS"<<endl;
  //SNIPPET
    
	upcxx::finalize(); 
}
