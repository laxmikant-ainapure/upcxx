#include <string>
#include <upcxx/upcxx.hpp>

struct S {
  int f;
  std::string s;
};

int main() {
  upcxx::init();

  S s;
  upcxx::global_ptr<int> gp = upcxx::new_<int>();
  upcxx::rput(0, gp,
    upcxx::remote_cx::as_rpc(
        [](S sr) {}, s));
  
  upcxx::finalize();
}
