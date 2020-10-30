// THIS IS A COMPILE-ONLY TEST
// This test is NOT guaranteed to exhibit correct runtime behavior!!
#include <upcxx/upcxx.hpp>

int nCount[2];
int phase = 1;

int main() {
  upcxx::init();

  auto f = [] (int phase){ nCount[phase]++;};
  upcxx::global_ptr<int> uL = upcxx::new_<int>();
  // void cast avoids a (justified) warning for this compile-only test
  (void)upcxx::rpc(0, f, phase);
  if ( uL ){
    rput( 4, uL, upcxx::remote_cx::as_rpc( f , phase) );
  } 

  upcxx::finalize();
  return 0;
} 
