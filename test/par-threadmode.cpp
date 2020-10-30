// Test for THREADMODE=par: UPCXX_THREADMODE must be defined and non-zero
#include <upcxx/upcxx.hpp>
#if !UPCXX_THREADMODE
  #error This test may only be compiled in PAR threadmode
#endif

int main() { return 0; }
