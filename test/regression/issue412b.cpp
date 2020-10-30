#include <upcxx/upcxx.hpp>
#include <iostream>
#include <stdlib.h>
#include "../util.hpp"

bool success = true;

using namespace upcxx;

int main(int argc, char **argv) {
    upcxx::init();

    int test = 0;
    if (argc > 1) test = std::atoi(argv[1]);

    print_test_header();

    dist_object<int> foo(1);
    atomic_domain<int> ad1({atomic_op::fetch_add});
    atomic_domain<int> ad2({atomic_op::fetch_add});
    atomic_domain<int> ad3({atomic_op::fetch_add});

    cuda_device dev1(cuda_device::invalid_device_id);
    cuda_device dev2(cuda_device::invalid_device_id);
    cuda_device dev3(cuda_device::invalid_device_id);

    team tm1 = world().split(0, 0);
    team tm2 = world().split(0, 0);
    team tm3 = world().split(0, 0);

    const int max_permitted = 11;
    for (int i=test; i==test || i <= max_permitted ; i++) {
      barrier();
      foo.fetch(rank_me()).then([&](int) { 
        #define CASE(i, action) case i: { \
          if (!rank_me()) std::cout << "invoking: " #action << std::endl; \
          volatile bool truth = true; /* avoid pedantic warning from PGI */ \
          if (truth) action; \
          break; }
        switch (i) {
          // permitted cases
          CASE(0, return barrier_async())
          CASE(1, dist_object<int> ok(0))
          CASE(2, dist_object<int> ok(world(),0))
          CASE(3, ad1.destroy(entry_barrier::internal))
          CASE(4, ad2.destroy(entry_barrier::none))
          CASE(5, tm1.destroy(entry_barrier::internal))
          CASE(6, tm2.destroy(entry_barrier::none))
          CASE(7, dev1.destroy(entry_barrier::internal))
          CASE(8, dev2.destroy(entry_barrier::none))
          CASE(9, return reduce_one<int>(1,op_fast_add,0).then([](int){}))
          CASE(10, return reduce_all<int>(1,op_fast_add).then([](int){}))
          CASE(11, return broadcast<int>(1,0).then([](int){}))

          // prohibited cases
          CASE(100, barrier())
          CASE(101, ad3.destroy())
          CASE(102, atomic_domain<int> ad4({atomic_op::fetch_add}))
          CASE(103, tm3.destroy())
          CASE(104, team tm4 = world().split(0, 0))
          CASE(105, dev3.destroy())
          CASE(106, cuda_device dev4(cuda_device::invalid_device_id))
          CASE(107, finalize())
          default:
            if (!rank_me()) std::cout << "unknown test: " << test << std::endl;
        }
        if (i < 0 || i > max_permitted) success = false;
        return make_future();
      }).wait();
    }

    print_test_success(success);

    upcxx::finalize();
    return 0;
}
