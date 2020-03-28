// Test for CODEMODE=debug: UPCXX_CODEMODE must be undefined
#include <upcxx/upcxx.hpp>
#ifdef UPCXX_CODEMODE
  #error This test may only be compiled in debug codemode
#endif

int main() { return 0; }
