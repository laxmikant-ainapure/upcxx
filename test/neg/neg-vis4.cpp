#include <upcxx/upcxx.hpp>
#include <tuple>

int main() {
  upcxx::init();

  long v = 0;
  std::pair<long *,int> sp(&v,1);
  using V = double;
  upcxx::global_ptr<V> gp = upcxx::new_<V>();
  std::pair<upcxx::global_ptr<V>,int> dp(gp,1);

  upcxx::rput_irregular(&sp,&sp+1, 
                        &dp,&dp+1).wait();
  
  upcxx::finalize();
}
