#include <upcxx/upcxx.hpp>
#include <iostream>
#include <chrono>

using namespace std;
using namespace upcxx;

int run_gg = 0;
int run_hg = 0;
int run_gh = 0;
int run_hh = 0;

template<typename src_ptr_type, typename dst_ptr_type, int flood>
static double helper(int warmup, int window_size, int trials, int len,
        src_ptr_type &src_ptr, dst_ptr_type &dst_ptr,
        int is_active_rank) {
    double elapsed = 0.0;

    if (is_active_rank) {
        for (int i = 0; i < warmup; i++) {
            upcxx::promise<> prom;
            for (int j = 0; j < window_size; j++) {
                upcxx::copy(src_ptr, dst_ptr, len,
                        upcxx::operation_cx::as_promise(prom));
                if (!flood) {
                    upcxx::future<> fut = prom.finalize();
                    fut.wait();
                    prom = upcxx::promise<>();
                }
            }

            upcxx::future<> fut = prom.finalize();
            fut.wait();
        }
    }

    upcxx::barrier();

    if (is_active_rank) {
        std::chrono::steady_clock::time_point start =
            std::chrono::steady_clock::now();

        for (int i = 0; i < trials; i++) {
            upcxx::promise<> prom;
            for (int j = 0; j < window_size; j++) {
                upcxx::copy(src_ptr, dst_ptr, len,
                        upcxx::operation_cx::as_promise(prom));
                if (!flood) {
                    upcxx::future<> fut = prom.finalize();
                    fut.wait();
                    prom = upcxx::promise<>();
                }
            }
            upcxx::future<> fut = prom.finalize();
            fut.wait();
        }

        std::chrono::steady_clock::time_point end =
            std::chrono::steady_clock::now();
        elapsed = std::chrono::duration<double>(end - start).count();
    }

    upcxx::barrier();

    return elapsed;
}

template<int flood>
static void run_all_copies(int warmup, int window_size, int trials, int msg_len,
        global_ptr<uint8_t,memory_kind::cuda_device> &local_gpu_array,
        global_ptr<uint8_t,memory_kind::cuda_device> &remote_gpu_array,
#ifdef USE_PRIVATE_SEGMENT
        uint8_t *local_host_array,
#else
        global_ptr<uint8_t> &local_host_array,
        global_ptr<uint8_t> &remote_host_array,
#endif
        int is_active_rank,
        double &local_gpu_to_remote_gpu, double &remote_gpu_to_local_gpu,
        double &local_host_to_remote_gpu, double &remote_gpu_to_local_host,
        double &local_gpu_to_remote_host, double &remote_host_to_local_gpu,
        double &local_host_to_remote_host, double &remote_host_to_local_host) {

#ifdef USE_PRIVATE_SEGMENT
    using host_ptr_type = uint8_t*;
#else
    using host_ptr_type = global_ptr<uint8_t>;
#endif

    if (run_gg) {
        local_gpu_to_remote_gpu =
            helper<upcxx::global_ptr<uint8_t, memory_kind::cuda_device>,
                   upcxx::global_ptr<uint8_t, memory_kind::cuda_device>, flood>(
                    warmup, window_size, trials, msg_len,
                    local_gpu_array, remote_gpu_array, is_active_rank);
        remote_gpu_to_local_gpu =
            helper<upcxx::global_ptr<uint8_t, memory_kind::cuda_device>,
                   upcxx::global_ptr<uint8_t, memory_kind::cuda_device>, flood>(
                    warmup, window_size, trials, msg_len,
                    remote_gpu_array, local_gpu_array, is_active_rank);
    }

    if (run_hg) {
        local_host_to_remote_gpu =
            helper<host_ptr_type, upcxx::global_ptr<uint8_t, memory_kind::cuda_device>, flood>(
                    warmup, window_size, trials, msg_len,
                    local_host_array, remote_gpu_array, is_active_rank);
        remote_gpu_to_local_host =
            helper<upcxx::global_ptr<uint8_t, memory_kind::cuda_device>, host_ptr_type, flood>(
                    warmup, window_size, trials, msg_len,
                    remote_gpu_array, local_host_array, is_active_rank);
    }

#ifndef USE_PRIVATE_SEGMENT
    if (run_gh) {
        local_gpu_to_remote_host =
            helper<upcxx::global_ptr<uint8_t, memory_kind::cuda_device>, host_ptr_type, flood>(
                    warmup, window_size, trials, msg_len, local_gpu_array,
                    remote_host_array, is_active_rank);
        remote_host_to_local_gpu =
            helper<host_ptr_type, upcxx::global_ptr<uint8_t, memory_kind::cuda_device>, flood>(
                    warmup, window_size, trials, msg_len, remote_host_array,
                    local_gpu_array, is_active_rank);
    }

    if (run_hh) {
        local_host_to_remote_host =
            helper<host_ptr_type, host_ptr_type, flood>(
                    warmup, window_size, trials, msg_len, local_host_array,
                    remote_host_array, is_active_rank);
        remote_host_to_local_host =
            helper<host_ptr_type, host_ptr_type, flood>(
                    warmup, window_size, trials, msg_len, remote_host_array,
                    local_host_array, is_active_rank);
    }
#endif
}

static void print_latency_results(double local_gpu_to_remote_gpu,
        double remote_gpu_to_local_gpu, double local_host_to_remote_gpu,
        double remote_gpu_to_local_host, double local_gpu_to_remote_host,
        double remote_host_to_local_gpu, double local_host_to_remote_host,
        double remote_host_to_local_host, int trials, int window_size) {
    long nmsgs = trials * window_size;

    std::cout << "Latency results for 8-byte transfers" << std::endl;

    if (run_gg) {
        std::cout << "  Local GPU -> Remote GPU: " <<
            (local_gpu_to_remote_gpu / double(nmsgs)) <<
            " s of latency" << std::endl;
        std::cout << "  Remote GPU -> Local GPU: " <<
            (remote_gpu_to_local_gpu / double(nmsgs)) <<
            " s of latency" << std::endl;
    }
    if (run_hg) {
        std::cout << "  Local Host -> Remote GPU: " <<
            (local_host_to_remote_gpu / double(nmsgs)) <<
            " s of latency" << std::endl;
        std::cout << "  Remote GPU -> Local Host: " <<
            (remote_gpu_to_local_host / double(nmsgs)) <<
            " s of latency" << std::endl;
    }
#ifndef USE_PRIVATE_SEGMENT
    if (run_gh) {
        std::cout << "  Local GPU -> Remote Host: " <<
            (local_gpu_to_remote_host / double(nmsgs)) <<
            " s of latency" << std::endl;
        std::cout << "  Remote Host -> Local GPU: " <<
            (remote_host_to_local_gpu / double(nmsgs)) <<
            " s of latency" << std::endl;
    }
    if (run_hh) {
        std::cout << "  Local Host -> Remote Host: " <<
            (local_host_to_remote_host / double(nmsgs)) <<
            " s of latency" << std::endl;
        std::cout << "  Remote Host -> Local Host: " <<
            (remote_host_to_local_host / double(nmsgs)) <<
            " s of latency" << std::endl;
    }
#endif
}

static void print_bandwidth_results(double local_gpu_to_remote_gpu,
        double remote_gpu_to_local_gpu, double local_host_to_remote_gpu,
        double remote_gpu_to_local_host, double local_gpu_to_remote_host,
        double remote_host_to_local_gpu, double local_host_to_remote_host,
        double remote_host_to_local_host, int trials, int window_size,
        int msg_len, int bidirectional, int flood) {
    std::string sync_type, dir_type;
    long nmsgs = trials * window_size;
    if (bidirectional) {
        nmsgs *= 2;
    }

    long nbytes = nmsgs * msg_len;
    double gbytes = double(nbytes) / (1024.0 * 1024.0 * 1024.0);

    if (flood) {
        sync_type = "Asynchronous";
    } else {
        sync_type = "Blocking";
    }

    if (bidirectional) {
        dir_type = "bi-directional";
    } else {
        dir_type = "uni-directional";
    }

    std::cout << sync_type << " " << dir_type << " bandwidth results for " <<
        "message size = " << msg_len << " byte(s)" << std::endl;

    if (run_gg) {
        std::cout << "  Local GPU -> Remote GPU: " <<
            (double(nmsgs) / local_gpu_to_remote_gpu) << " msgs/s, " <<
            (double(gbytes) / local_gpu_to_remote_gpu) << " GB/s" <<
            std::endl;
        std::cout << "  Remote GPU -> Local GPU: " <<
            (double(nmsgs) / remote_gpu_to_local_gpu) << " msgs/s, " <<
            (double(gbytes) / remote_gpu_to_local_gpu) << " GB/s" <<
            std::endl;
    }
    if (run_hg) {
        std::cout << "  Local Host -> Remote GPU: " <<
            (double(nmsgs) / local_host_to_remote_gpu) << " msgs/s, " <<
            (double(gbytes) / local_host_to_remote_gpu) << " GB/s" <<
            std::endl;
        std::cout << "  Remote GPU -> Local Host: " <<
            (double(nmsgs) / remote_gpu_to_local_host) << " msgs/s, " <<
            (double(gbytes) / remote_gpu_to_local_host) << " GB/s" <<
            std::endl;
    }
#ifndef USE_PRIVATE_SEGMENT
    if (run_gh) {
        std::cout << "  Local GPU -> Remote Host: " <<
            (double(nmsgs) / local_gpu_to_remote_host) << " msgs/s, " <<
            (double(gbytes) / local_gpu_to_remote_host) << " GB/s" <<
            std::endl;
        std::cout << "  Remote Host -> Local GPU: " <<
            (double(nmsgs) / remote_host_to_local_gpu) << " msgs/s, " <<
            (double(gbytes) / remote_host_to_local_gpu) << " GB/s" <<
            std::endl;
    }
    if (run_hh) {
        std::cout << "  Local Host -> Remote Host: " <<
            (double(nmsgs) / local_host_to_remote_host) << " msgs/s, " <<
            (double(gbytes) / local_host_to_remote_host) << " GB/s" <<
            std::endl;
        std::cout << "  Remote Host -> Local Host: " <<
            (double(nmsgs) / remote_host_to_local_host) << " msgs/s, " <<
            (double(gbytes) / remote_host_to_local_host) << " GB/s" <<
            std::endl;
    }
#endif
}

int main(int argc, char **argv) {
   {
       upcxx::init();

       const int partner = (rank_me() + 1) % rank_n();
       int max_msg_size = 4 * 1024 * 1024; // 4MB
       std::size_t segsize = max_msg_size;
       auto gpu_device = upcxx::cuda_device( 0 ); // open device 0

       int warmup = 10;
       int trials = 100;
       int window_size = 100;

       int arg_index = 1;
       while (arg_index < argc) {
           char *arg = argv[arg_index];
           if (strcmp(arg, "-t") == 0) {
               if (arg_index + 1 == argc) {
                   if (!rank_me()) fprintf(stderr, "Missing argument to -t\n");
                   upcxx::finalize();
                   return 1;
               }
               arg_index++;
               trials = atoi(argv[arg_index]);
           } else if (strcmp(arg, "-w") == 0) {
               if (arg_index + 1 == argc) {
                   if (!rank_me()) fprintf(stderr, "Missing argument to -w\n");
                   upcxx::finalize();
                   return 1;
               }
               arg_index++;
               window_size = atoi(argv[arg_index]);
           } else if (strcmp(arg, "-gg") == 0) {
               run_gg = 1;
           } else if (strcmp(arg, "-hg") == 0) {
               run_hg = 1;
           } else if (strcmp(arg, "-gh") == 0) {
               run_gh = 1;
           } else if (strcmp(arg, "-hh") == 0) {
               run_hh = 1;
           } else {
               if (!rank_me()) {
                   fprintf(stderr, "usage: %s [-t trials] [-w window] [-gg] [-hg]"
#ifndef USE_PRIVATE_SEGMENT
                           " [-gh] [-hh]"
#endif
                           "\n", argv[0]);
                   fprintf(stderr, "       -gg: Run tests between local and remote GPU memory\n");
                   fprintf(stderr, "       -hg: Run tests between local host and remote GPU memory\n");
#ifndef USE_PRIVATE_SEGMENT
                   fprintf(stderr, "       -gh: Run tests between local GPU and remote host memory\n");
                   fprintf(stderr, "       -hh: Run tests between local and remote host memory\n");
#endif
               }
               upcxx::finalize();
               return 1;
           }
           arg_index++;
       }

       if (!run_gg && !run_hg && !run_gh && !run_hh) {
           // If no tests are selected at the command line, run them all
           run_gg = run_hg = run_gh = run_hh = 1;
       }

       if (rank_me() == 0) {
           std::cout << "Running " << trials << " trials of window_size=" <<
               window_size << std::endl;
#ifdef USE_PRIVATE_SEGMENT
           std::cout << "Running using the private segment" << std::endl;
#endif
       }


       // alloc GPU segment
       auto gpu_alloc = upcxx::device_allocator<upcxx::cuda_device>(gpu_device,
               segsize);

       global_ptr<uint8_t,memory_kind::cuda_device> local_gpu_array =
           gpu_alloc.allocate<uint8_t>(max_msg_size);

       upcxx::dist_object<upcxx::global_ptr<uint8_t, memory_kind::cuda_device>> gpu_dobj(local_gpu_array);
       global_ptr<uint8_t, memory_kind::cuda_device> remote_gpu_array =
           gpu_dobj.fetch(partner).wait();

#ifdef USE_PRIVATE_SEGMENT
       uint8_t *host_array = new uint8_t[max_msg_size];
#else
       global_ptr<uint8_t, memory_kind::host> host_array =
           upcxx::new_array<uint8_t>(max_msg_size);
       upcxx::dist_object<upcxx::global_ptr<uint8_t>> host_dobj(host_array);
       global_ptr<uint8_t> remote_host_array = host_dobj.fetch(partner).wait();
#endif

       double local_gpu_to_remote_gpu, remote_gpu_to_local_gpu,
              local_host_to_remote_gpu, remote_gpu_to_local_host,
              local_gpu_to_remote_host, remote_host_to_local_gpu,
              local_host_to_remote_host, remote_host_to_local_host;

       run_all_copies<0>(warmup, window_size, trials, 8, local_gpu_array,
               remote_gpu_array, host_array,
#ifndef USE_PRIVATE_SEGMENT
               remote_host_array,
#endif
               (rank_me() == 0),
               local_gpu_to_remote_gpu, remote_gpu_to_local_gpu,
               local_host_to_remote_gpu, remote_gpu_to_local_host,
               local_gpu_to_remote_host, remote_host_to_local_gpu,
               local_host_to_remote_host, remote_host_to_local_host);

       if (rank_me() == 0) {
           print_latency_results(local_gpu_to_remote_gpu,
                   remote_gpu_to_local_gpu, local_host_to_remote_gpu,
                   remote_gpu_to_local_host, local_gpu_to_remote_host,
                   remote_host_to_local_gpu, local_host_to_remote_host,
                   remote_host_to_local_host, trials, window_size);
           std::cout << std::endl;
       }

       upcxx::barrier();

       int msg_len = 1;
       while (msg_len <= max_msg_size) {
           // Uni-directional blocking bandwidth test
           int is_active_rank = !(rank_me() & 1);
           run_all_copies<0>(warmup, window_size, trials, msg_len,
                   local_gpu_array, remote_gpu_array, host_array,
#ifndef USE_PRIVATE_SEGMENT
                   remote_host_array,
#endif
                   is_active_rank,
                   local_gpu_to_remote_gpu, remote_gpu_to_local_gpu,
                   local_host_to_remote_gpu, remote_gpu_to_local_host,
                   local_gpu_to_remote_host, remote_host_to_local_gpu,
                   local_host_to_remote_host, remote_host_to_local_host);

           if (rank_me() == 0) {
               print_bandwidth_results(local_gpu_to_remote_gpu,
                       remote_gpu_to_local_gpu, local_host_to_remote_gpu,
                       remote_gpu_to_local_host, local_gpu_to_remote_host,
                       remote_host_to_local_gpu, local_host_to_remote_host,
                       remote_host_to_local_host, trials, window_size,
                       msg_len, 0, 0);
               std::cout << std::endl;
           }
           
           upcxx::barrier();

           // Uni-directional non-blocking bandwidth test
           run_all_copies<1>(warmup, window_size, trials, msg_len,
                   local_gpu_array, remote_gpu_array, host_array,
#ifndef USE_PRIVATE_SEGMENT
                   remote_host_array,
#endif
                   is_active_rank,
                   local_gpu_to_remote_gpu, remote_gpu_to_local_gpu,
                   local_host_to_remote_gpu, remote_gpu_to_local_host,
                   local_gpu_to_remote_host, remote_host_to_local_gpu,
                   local_host_to_remote_host, remote_host_to_local_host);

           if (rank_me() == 0) {
               print_bandwidth_results(local_gpu_to_remote_gpu,
                       remote_gpu_to_local_gpu, local_host_to_remote_gpu,
                       remote_gpu_to_local_host, local_gpu_to_remote_host,
                       remote_host_to_local_gpu, local_host_to_remote_host,
                       remote_host_to_local_host, trials, window_size,
                       msg_len, 0, 1);
               std::cout << std::endl;
           }

           upcxx::barrier();

           // Bi-directional blocking bandwidth test
           is_active_rank = 1;
           run_all_copies<0>(warmup, window_size, trials, msg_len,
                   local_gpu_array, remote_gpu_array, host_array,
#ifndef USE_PRIVATE_SEGMENT
                   remote_host_array,
#endif
                   is_active_rank,
                   local_gpu_to_remote_gpu, remote_gpu_to_local_gpu,
                   local_host_to_remote_gpu, remote_gpu_to_local_host,
                   local_gpu_to_remote_host, remote_host_to_local_gpu,
                   local_host_to_remote_host, remote_host_to_local_host);

           if (rank_me() == 0) {
               print_bandwidth_results(local_gpu_to_remote_gpu,
                       remote_gpu_to_local_gpu, local_host_to_remote_gpu,
                       remote_gpu_to_local_host, local_gpu_to_remote_host,
                       remote_host_to_local_gpu, local_host_to_remote_host,
                       remote_host_to_local_host, trials, window_size,
                       msg_len, 1, 0);
               std::cout << std::endl;
           }
           
           upcxx::barrier();

           // Bi-directional non-blocking bandwidth test
           run_all_copies<1>(warmup, window_size, trials, msg_len,
                   local_gpu_array, remote_gpu_array, host_array,
#ifndef USE_PRIVATE_SEGMENT
                   remote_host_array,
#endif
                   is_active_rank,
                   local_gpu_to_remote_gpu, remote_gpu_to_local_gpu,
                   local_host_to_remote_gpu, remote_gpu_to_local_host,
                   local_gpu_to_remote_host, remote_host_to_local_gpu,
                   local_host_to_remote_host, remote_host_to_local_host);

           if (rank_me() == 0) {
               print_bandwidth_results(local_gpu_to_remote_gpu,
                       remote_gpu_to_local_gpu, local_host_to_remote_gpu,
                       remote_gpu_to_local_host, local_gpu_to_remote_host,
                       remote_host_to_local_gpu, local_host_to_remote_host,
                       remote_host_to_local_host, trials, window_size,
                       msg_len, 1, 1);
               std::cout << std::endl;
           }

           upcxx::barrier();

           msg_len *= 2;
       }

       gpu_alloc.deallocate(local_gpu_array);
#ifdef USE_PRIVATE_SEGMENT
       delete[] host_array;
#else
       upcxx::delete_array(host_array);
#endif
       gpu_device.destroy();

       upcxx::barrier();

       if (!rank_me())  std::cout << "SUCCESS" << std::endl;
   }
   upcxx::finalize();
}
