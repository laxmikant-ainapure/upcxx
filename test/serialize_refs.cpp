#include <string>
#include <tuple>
#include <type_traits>
#include <upcxx/upcxx.hpp>

template<typename T, typename U>
struct assert_same {
  static_assert(std::is_same<T, U>::value, "differ");
};

int z = 5;
std::string s = "test";

int main() {
  upcxx::init();

  int x = 3;
  double y = 5.3;

  {
    auto res = upcxx::rpc((upcxx::rank_me() + 1) % upcxx::rank_n(),
      [=](std::pair<int, double> q) -> std::string&& {
        UPCXX_ASSERT_ALWAYS(q.first == x && q.second == -y);
        return std::move(s);
      },
      std::pair<const int&, double&&>(x, -y));
    assert_same<decltype(res), upcxx::future<std::string>>{};
    UPCXX_ASSERT_ALWAYS(res.wait() == "test");
  }

  {
    auto res = upcxx::rpc((upcxx::rank_me() + 1) % upcxx::rank_n(),
      [=]() -> const int& {
        return z;
      });
    assert_same<decltype(res), upcxx::future<int>>{};
    UPCXX_ASSERT_ALWAYS(res.wait() == z);
  }

  {
    auto res = upcxx::rpc((upcxx::rank_me() + 1) % upcxx::rank_n(),
      [=]() -> int& {
        return z;
      });
    assert_same<decltype(res), upcxx::future<int>>{};
    UPCXX_ASSERT_ALWAYS(res.wait() == z);
  }

  upcxx::finalize();
}
