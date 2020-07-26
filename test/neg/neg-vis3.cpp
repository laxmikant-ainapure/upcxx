#include <upcxx/upcxx.hpp>
#include <vector>
#include <tuple>

int main() {
  upcxx::init();

  std::vector<int> v;
  v.resize(100);
  std::pair<std::vector<int> *,int> sp(&v,1);
  upcxx::global_ptr<std::vector<int>> gp = upcxx::new_<std::vector<int>>();
  std::pair<upcxx::global_ptr<std::vector<int>>,int> dp(gp,1);

  upcxx::rput_irregular(&sp,&sp+1, 
                        &dp,&dp+1).wait();
  
  upcxx::finalize();
}
