#include <upcxx/upcxx.hpp>
#include "../util.hpp"

struct T {
  bool valid = true;
  ~T() { valid = false; }
};

T global;

int main() {
  upcxx::init();
  print_test_header();

  upcxx::persona &target = upcxx::current_persona();
  upcxx::future<T const&> f =
    target.lpc([]() -> T const & { return global; });
  UPCXX_ASSERT_ALWAYS(f.wait_reference().valid);
  UPCXX_ASSERT_ALWAYS(&f.wait_reference() == &global);

  print_test_success();
  upcxx::finalize();
}
