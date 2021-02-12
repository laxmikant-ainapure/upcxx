#include <upcxx/upcxx.hpp>
#include <thread>
#include <sstream>
#include <vector>
#include "util.hpp"

// NOTE: This test is carefully written to be safe in either SEQ or PAR THREADMODE
// See the rules documented in docs/implementation-defined.md

#ifndef WORDS
#define WORDS 64
#endif

using std::uint64_t;

long done;

struct msg {
  uint64_t words[WORDS];
  uint64_t sum;
  msg(uint64_t base) { // init with arithmetically related values
    sum = 0;
    for (int i=0; i<WORDS; i++) {
      uint64_t v = base + i + 1;
      sum += v;
      words[i] = v;
    }
  }
  void operator()() {
    msg tmp = *this; // read all
    uint64_t check = 0;
    for (int i=0; i<WORDS; i++) {
      check += tmp.words[i]; // validate
    }
    if (check != tmp.sum || check <= 0) {
      std::ostringstream oss;
      oss << "ERROR: Data corruption detected: proc="<<upcxx::rank_me() << " sum=" << tmp.sum << " words: ";
      for (int i=0; i<WORDS; i++) oss << tmp.words[i] << " ";
      oss << "\n";
      std::cerr << oss.str() << std::flush;
      std::abort();
    }
    done--;
  }
};

int main (int argc, char ** argv) {
  upcxx::init();

  print_test_header();

  long iters = 0;
  int threads = 0;

  if (argc > 1) iters = atol(argv[1]);
  if (iters <= 0) iters = 10000;

  if (argc > 2) threads = atol(argv[2]);
  if (threads <= 0) threads = 10;

  if (!upcxx::rank_me()) 
    std::cout << "iters = " << iters 
              << " threads = " << threads 
              << " WORDS = " << WORDS << std::endl;

  done = iters * threads;

  upcxx::barrier();

  std::atomic<int> go(0);

  std::vector<std::thread *> th;

  for (int i=0; i < threads; i++) {
    th.push_back( new std::thread([&]() {
      while (!go.load(std::memory_order_relaxed)) sched_yield();

      for (long i=0; i<iters; i++) {
        upcxx::master_persona().lpc(msg(i)).wait();
      }

    }));
  }

  go.store(1, std::memory_order_relaxed);
  while (done) {
    upcxx::progress();
  }

  for (auto t : th) {
    t->join();
    delete t;
  }
  upcxx::barrier();

  print_test_success();

  upcxx::finalize();

  return 0;
}

