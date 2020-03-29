// Test for CODEMODE=par: UPCXX_CODEMODE must be defined and non-zero
#include <upcxx/upcxx.hpp>
#if !UPCXX_CODEMODE
  #error This test may only be compiled in O3 (production) codemode
#endif

int main() { return 0; }
