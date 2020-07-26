#include <upcxx/upcxx.hpp>
#include <vector>

int main() {
  upcxx::init();
 
  std::vector<int> v;
  std::vector<int> *vp = &v;
  v.resize(100);
  upcxx::global_ptr<std::vector<int>> gp = upcxx::new_<std::vector<int>>();

  upcxx::rput_regular(&vp,&vp+1,1, 
                      &gp,&gp+1,1).wait();
  
  upcxx::finalize();
}
