#include <upcxx/upcxx.hpp>

int main() {
  upcxx::init();
 
  long v;
  long *vp = &v;
  using V = double;
  upcxx::global_ptr<V> gp = upcxx::new_<V>();

  upcxx::rget_regular(&gp,&gp+1,1, 
                      &vp,&vp+1,1).wait();
  
  upcxx::finalize();
}
