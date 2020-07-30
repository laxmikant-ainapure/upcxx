#include <string>
#include <tuple>
#include <type_traits>
#include <upcxx/upcxx.hpp>

#include "util.hpp"

template<typename T, bool TS, bool S, typename DT = void>
struct check {
  static_assert(upcxx::is_trivially_serializable<T>::value == TS, "ERROR");
  static_assert(upcxx::is_serializable<T>::value == S, "ERROR");
  static_assert(std::is_same<upcxx::deserialized_type_t<T>, DT>::value, "ERROR");
};
template<typename T>
struct check<T, false, false, void> {
  static_assert(!upcxx::is_trivially_serializable<T>::value, "ERROR");
  static_assert(!upcxx::is_serializable<T>::value, "ERROR");
};

struct D {
  int w,x;
  ~D() {} // break trivialness
};
struct S { // asymmetric serialization
  int x,y,z;
  struct upcxx_serialization {
    template<typename Writer>
    static void serialize (Writer& writer, S const & t) {
        writer.write(t.x);
    }
    template<typename Reader>
    static D* deserialize(Reader& reader, void* storage) {
        int x = reader.template read<int>();
        D *up = new(storage) D();
        up->x = x;
        return up;
    }
  };
};

namespace upcxx {
  template<>
  struct is_trivially_serializable<::D> : ::std::true_type {};
};

int z = 5;
std::string s = "test";
S t{-1, -2, -3};
D d{-4, -5};

int main() {
  upcxx::init();

  print_test_header();

  int x = 3;
  double y = 5.3;

  check<const int, true, true, const int>{};
  check<const int&, false, true, int>{};
  check<int&&, false, true, int>{};

  check<const std::string, false, true, const std::string>{};
  check<const std::string&, false, true, std::string>{};
  check<std::string&&, false, true, std::string>{};

  check<volatile std::string, false, false>{};
  check<const volatile std::string, false, false>{};
  check<volatile std::string&, false, false>{};
  check<const volatile std::string&, false, false>{};

  check<S, false, true, D>{};
  check<const S, false, true, const D>{};
  check<const S&, false, true, D>{};
  check<S&&, false, true, D>{};

  check<D, true, true, D>{};
  check<const D, true, true, const D>{};
  check<const D&, false, true, D>{};
  check<D&&, false, true, D>{};

  check<std::pair<const int, const D>, true, true, std::pair<const int, const D>>{};
  check<std::tuple<const int, const S&, const D&>, false, true, std::tuple<const int, D, D>>{};
  check<std::tuple<const int&, const S, const D>, false, true, std::tuple<int, const D, const D>>{};

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

#if 0 // broken due to issue 394
  {
    auto res = upcxx::rpc((upcxx::rank_me() + 1) % upcxx::rank_n(),
      [=](std::pair<const D, const D> p) -> const S& {
        UPCXX_ASSERT_ALWAYS(p.first.x == 1);
        UPCXX_ASSERT_ALWAYS(p.second.w == 4);
        UPCXX_ASSERT_ALWAYS(p.second.x == 5);
        return t;
      },
      std::pair<const S, const D>{S{1, 2, 3}, D{4, 5}});
    assert_same<decltype(res), upcxx::future<D>>{};
    UPCXX_ASSERT_ALWAYS(res.wait().x == -1);
  }
#endif

  {
    auto res = upcxx::rpc((upcxx::rank_me() + 1) % upcxx::rank_n(),
      [=](std::pair<D, D> p) -> D&& {
        UPCXX_ASSERT_ALWAYS(p.first.x == 1);
        UPCXX_ASSERT_ALWAYS(p.second.w == 4);
        UPCXX_ASSERT_ALWAYS(p.second.x == 5);
        return std::move(d);
      },
      std::pair<const S&, D&&>{S{1, 2, 3}, D{4, 5}});
    assert_same<decltype(res), upcxx::future<D>>{};
    UPCXX_ASSERT_ALWAYS(res.wait().w == -4);
    UPCXX_ASSERT_ALWAYS(res.wait().x == -5);
  }

  print_test_success();

  upcxx::finalize();
}
