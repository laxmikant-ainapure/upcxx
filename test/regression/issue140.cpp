#include <upcxx/upcxx.hpp>
#include <iostream>
#include "../util.hpp"
#include <gasnet.h>

using namespace std;
using namespace upcxx;

int main() {
    upcxx::init();
    print_test_header();

    if (rank_n() < 2) {
      cout << "SKIPPED: Test requires 2 or more ranks." << endl;
      print_test_success(true);
      upcxx::finalize();
      return 0;
    }

    static future<> done;
    dist_object<global_ptr<int>> dobj(new_array<int>(rank_n()));
    global_ptr<int> gptr = *dobj;
    if (rank_me() == 0) {
      // fetch remote landing zones
      auto LZ = new global_ptr<int>[rank_n()];
      for (int i=1; i < rank_n(); i++) {
        LZ[i] = dobj.fetch(i).wait();
      }
      // inject rput-then-rpc to each peer
      for (int i=1; i < rank_n(); i++) {
        rput(&i, LZ[i], 1, remote_cx::as_rpc([](global_ptr<int> gptr) {
          done = rput(rank_me(), gptr);
        },gptr+i));
      }
      // this *should* ensure all the outgoing RPCs above are sent
      upcxx::discharge(); 
      // now wait for the returning puts
      for (int i=1; i < rank_n(); i++) {
        int *ptr = gptr.local() + i;
        while (!*ptr) { 
           #ifdef FORCE_PROGRESS
             upcxx::progress(progress_level::internal);
           #endif
           // simulate computational work that does not progress UPC++
           gasnet_AMPoll();
           gasnett_sched_yield();
        }
      }
    } else {
      while (!done.ready()) progress();
    }

    barrier();

    print_test_success(true);
    upcxx::finalize();
    return 0;
} 
