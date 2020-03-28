// Test for THREADMODE=seq: UPCXX_THREADMODE must be undefined
#include <upcxx/upcxx.hpp>
#ifdef UPCXX_THREADMODE
  #error This test may only be compiled in SEQ threadmode
#endif

int main() { return 0; }
