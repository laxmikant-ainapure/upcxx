#include <upcxx/serialization.hpp>
#include <upcxx/utility.hpp>
#include "util.hpp"

using namespace std;
using namespace upcxx;

template<typename T>
bool equals(T const &a, T const &b) {
  return a == b;
}

template<typename T, std::size_t n>
bool equals(T const (&a)[n], T const (&b)[n]) {
  bool ans = true;
  for(std::size_t i=0; i != n; i++)
    ans &= equals(a[i], b[i]);
  return ans;
}

template<typename T>
void roundtrip(T const &x) {
  alignas(64) char buf0[128];
  detail::serialization_writer<
      decltype(serialization_complete<T>::static_ubound)::is_valid
    > w(buf0, sizeof(buf0));

  serialization_complete<T>::serialize(w, x);

  void *buf1 = upcxx::detail::alloc_aligned(w.size(), std::max(sizeof(void*), w.align()));
  w.compact_and_invalidate(buf1);
  
  detail::serialization_reader r(buf1);
  
  typename std::aligned_storage<sizeof(T),alignof(T)>::type x1_;
  T *x1 = serialization_complete<T>::deserialize(r, &x1_);

  UPCXX_ASSERT_ALWAYS(equals(*x1, x), "Serialization roundtrip failed. sizeof(T)="<<sizeof(T)<<" is_def_triv_serz="<<is_definitely_trivially_serializable<T>::value);

  upcxx::detail::destruct(*x1);
  std::free(buf1);
}

struct nonpod {
  char h;
  char i;

  nonpod() = default;
  nonpod(char h, char i):
    h(h), i(i) {
  }
  nonpod(nonpod const &that):
    h(that.h),
    i(that.i) {
  }

  bool operator==(nonpod that) const {
    return h==that.h && i==that.i;
  }

  #if 1
    UPCXX_SERIALIZED_FIELDS(h,i)
  #else
    struct serialization {
      template<typename Prefix>
      static auto ubound(Prefix pre, nonpod const &x)
        -> decltype(pre.cat_ubound_of(x.h).cat_ubound_of(x.i)) {
        return pre.cat_ubound_of(x.h).cat_ubound_of(x.i);
      }
      template<typename Writer>
      static void serialize(Writer &w, nonpod const &x) {
        w.push(0xbeef);
        w.push(x.h);
        w.push(x.i);
      }
      template<typename Reader>
      static nonpod* deserialize(Reader &r, void *spot) {
        UPCXX_ASSERT_ALWAYS(r.template pop<int>() == 0xbeef);
        char h = r.template pop<char>();
        char i = r.template pop<char>();
        return ::new(spot) nonpod(h,i);
      }
    };
  #endif
};

int main() {
  roundtrip<std::int8_t>(1);
  roundtrip<std::uint16_t>(1000);
  roundtrip<std::int32_t>(1<<29);
  roundtrip<std::uint32_t>(1<<31);
  roundtrip<float>(3.14f);
  roundtrip<double>(3.14);
  roundtrip(nonpod('h','i'));
  roundtrip(std::array<int,10>{{0,1,2,3,4,5,6,7,8,9}});
  roundtrip<int[10]>({0,1,2,3,4,5,6,7,8,9});
  roundtrip<nonpod[3]>({{'a','b'}, {'x','y'}, {'u','v'}});
  roundtrip(std::make_pair('a', 1));
  roundtrip(std::make_pair('a', nonpod('h','i')));
  roundtrip(std::make_tuple('a', 1, 3.14));
  roundtrip(std::make_tuple('a', 1, 3.14, std::string("abcdefghijklmnopqrstuvwxyz")));
  roundtrip(std::vector<int>{1,2,3});
  roundtrip(std::deque<nonpod>{{'a','b'}, {'x','y'}});
  roundtrip(std::vector<std::list<std::tuple<std::string,nonpod>>>{{}, {{"hi",{'a','b'}}, {"bob",{'x','y'}}}});

  {
    std::unordered_map<int, std::pair<int,std::string>> m;
    for(int i=0; i < 1000; i++)
      m[i] = {i, std::string(11*i,'x')};
    roundtrip(m);
  }
  {
    std::map<int, std::pair<int,std::string>> m;
    for(int i=0; i < 1000; i++)
      m[i] = {i, std::string(11*i,'x')};
    roundtrip(m);
  }
  {
    std::multiset<std::pair<int,std::string>> m;
    for(int i=0; i < 1000; i++)
      m.insert({i, std::string(11*i,'x')});
    roundtrip(m);
  }
  return 0;
}
