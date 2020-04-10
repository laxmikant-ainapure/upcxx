// Test for CODEMODE=opt: UPCXX_CODEMODE must be defined and non-zero
#include <upcxx/upcxx.hpp>
#if !UPCXX_CODEMODE
  #error This test may only be compiled in opt (production) codemode
#endif

int main() { return 0; }
