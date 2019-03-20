#include <upcxx/upcxx.hpp>
#include <iostream>
#include <chrono>

using namespace std;
using namespace upcxx;

template<upcxx::memory_kind src_memory_kind, upcxx::memory_kind dst_memory_kind>
static double helper(int warmup, int window_size, int trials, int len,
        global_ptr<uint8_t, src_memory_kind> &src_ptr,
        global_ptr<uint8_t, dst_memory_kind> &dst_ptr,
        int is_active_rank) {
    upcxx::future<> all = upcxx::make_future();

    if (is_active_rank) {
        for (int i = 0; i < warmup; i++) {
            for (int j = 0; j < window_size; j++) {
                upcxx::future<> fut = upcxx::copy(src_ptr, dst_ptr, len);
#ifdef COPY_ASYNC
                all = upcxx::when_all(all, fut);
#else
                fut.wait();
#endif
            }

            all.wait(); // no-op if !COPY_ASYNC
        }
    }

    upcxx::barrier();

    if (is_active_rank) {
        upcxx::future<> all = upcxx::make_future();

        std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();

        for (int i = 0; i < trials; i++) {
            for (int j = 0; j < window_size; j++) {
                upcxx::future<> fut = upcxx::copy(src_ptr, dst_ptr, len);
#ifdef COPY_ASYNC
                all = upcxx::when_all(all, fut);
#else
                fut.wait();
#endif
            }

            all.wait(); // no-op if !COPY_ASYNC
        }

        std::chrono::steady_clock::time_point end =
            std::chrono::steady_clock::now();
        return std::chrono::duration<double>(end - start).count();
    }

    return 0;
}

int main(int argc, char **argv) {
   {
       upcxx::init();

       int is_active_rank = (rank_me() == 0);
       int partner = (rank_me() + 1) % rank_n();

       int max_msg_size = 4 * 1024 * 1024; // 4MB
       std::size_t segsize = max_msg_size;
       auto gpu_device = upcxx::cuda_device( 0 ); // open device 0

       // alloc GPU segment
       auto gpu_alloc = upcxx::device_allocator<upcxx::cuda_device>(gpu_device,
               segsize);

       global_ptr<uint8_t,memory_kind::cuda_device> local_gpu_array =
           gpu_alloc.allocate<uint8_t>(max_msg_size);
       global_ptr<uint8_t, memory_kind::host> host_array =
           upcxx::new_array<uint8_t>(max_msg_size);

       upcxx::dist_object<upcxx::global_ptr<uint8_t, memory_kind::cuda_device>> dobj(local_gpu_array);
       global_ptr<uint8_t, memory_kind::cuda_device> remote_gpu_array =
           dobj.fetch(partner).wait();

       int warmup = 10;
       int trials = 100;
       int window_size = 100;

       if (argc > 1) trials = atoi(argv[1]);
       if (argc > 2) window_size = atoi(argv[2]);
       if (!rank_me())
         std::cout << "Running " << trials << " trials of window_size=" << window_size << std::endl;

       // TODO bi-directional and uni-directional

       int len = 1;
       while (len <= max_msg_size) {

           double local_gpu_to_remote_gpu = helper(warmup, window_size, trials,
                   len, local_gpu_array, remote_gpu_array, is_active_rank);
           double remote_gpu_to_local_gpu = helper(warmup, window_size, trials,
                   len, remote_gpu_array, local_gpu_array, is_active_rank);
           double local_host_to_remote_gpu = helper(warmup, window_size, trials,
                   len, host_array, remote_gpu_array, is_active_rank);
           double remote_gpu_to_local_host = helper(warmup, window_size, trials,
                   len, remote_gpu_array, host_array, is_active_rank);

           if (is_active_rank) {
               long nmsgs = trials * window_size;
               long nbytes = nmsgs * len;
               double gbytes = double(nbytes) / (1024.0 * 1024.0 * 1024.0);

               std::cout << "Message size = " << len << " byte(s)" << std::endl;
               std::cout << "  Local GPU -> Remote GPU: " <<
                   (double(nmsgs) / local_gpu_to_remote_gpu) << " msgs/s, " <<
                   (double(gbytes) / local_gpu_to_remote_gpu) << " GB/s" <<
                   std::endl;
               std::cout << "  Remote GPU -> Local GPU: " <<
                   (double(nmsgs) / remote_gpu_to_local_gpu) << " msgs/s, " <<
                   (double(gbytes) / remote_gpu_to_local_gpu) << " GB/s" <<
                   std::endl;
               std::cout << "  Local Host -> Remote GPU: " <<
                   (double(nmsgs) / local_host_to_remote_gpu) << " msgs/s, " <<
                   (double(gbytes) / local_host_to_remote_gpu) << " GB/s" <<
                   std::endl;
               std::cout << "  Remote GPU -> Local Host: " <<
                   (double(nmsgs) / remote_gpu_to_local_host) << " msgs/s, " <<
                   (double(gbytes) / remote_gpu_to_local_host) << " GB/s" <<
                   std::endl;
               std::cout << std::endl;
           }

           upcxx::barrier();

           len *= 2;
       }

       gpu_alloc.deallocate(local_gpu_array);
       upcxx::delete_array(host_array);
       gpu_device.destroy();


       upcxx::barrier();

       if (!rank_me())  std::cout << "SUCCESS" << std::endl;
   }
   upcxx::finalize();
}
