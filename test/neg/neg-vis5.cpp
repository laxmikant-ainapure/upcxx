#include <upcxx/upcxx.hpp>
#include <vector>
#include <tuple>
#include <array>

int main() {
  upcxx::init();

  std::vector<int> v;
  v.resize(100);
  upcxx::global_ptr<std::vector<int>> gp = upcxx::new_<std::vector<int>>();

  upcxx::rput_strided(&v,std::array<std::ptrdiff_t,1>{sizeof(v)},
                      gp,std::array<std::ptrdiff_t,1>{sizeof(v)},
                      std::array<std::size_t,1>{1}).wait();
  
  upcxx::finalize();
}
