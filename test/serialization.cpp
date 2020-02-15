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
  void *buf0 = upcxx::detail::alloc_aligned(8*serialization_align_max, serialization_align_max);
  
  detail::serialization_writer<
      decltype(serialization_traits<T>::static_ubound)::is_valid
    > w(buf0, 8*serialization_align_max);

  serialization_traits<T>::serialize(w, x);

  void *buf1 = upcxx::detail::alloc_aligned(w.size(), std::max(sizeof(void*), w.align()));
  w.compact_and_invalidate(buf1);
  
  detail::serialization_reader r(buf1);
  
  typename std::aligned_storage<sizeof(T),alignof(T)>::type x1_;
  T *x1 = serialization_traits<T>::deserialize(r, &x1_);

  const bool is_triv = is_trivially_serializable<T>::value; // workaround a bug in Xcode 8.2.1
  UPCXX_ASSERT_ALWAYS(equals(*x1, x), "Serialization roundtrip failed. sizeof(T)="<<sizeof(T)<<" is_triv_serz="<<is_triv);

  upcxx::detail::destruct(*x1);
  std::free(buf1);
  std::free(buf0);
}

struct nonpod_base {
  char h, i;
  nonpod_base **mirror;
  
  nonpod_base() {
    mirror = new nonpod_base*(this);
  }
  nonpod_base(char h, char i): nonpod_base() {
    this->h = h;
    this->i = i;
  }
  nonpod_base(const nonpod_base &that): nonpod_base() {
    this->h = that.h;
    this->i = that.i;
  }

  ~nonpod_base() {
    UPCXX_ASSERT_ALWAYS(*mirror == this);
    delete mirror;
  }
  
  bool operator==(nonpod_base const &that) const {
    UPCXX_ASSERT_ALWAYS(*this->mirror == this);
    UPCXX_ASSERT_ALWAYS(*that.mirror == &that);
    return this->h == that.h && this->i == that.i;
  }
};

struct nonpod1: nonpod_base {
  nonpod1() = default;
  nonpod1(char h, char i): nonpod_base(h,i) {}
  UPCXX_SERIALIZED_FIELDS(h,i)
};

struct nonpod2: nonpod_base {
  nonpod2(char h, char i): nonpod_base(h,i) {}
  nonpod2(char h, char i, char const *dummy): nonpod_base(h,i) {}
  UPCXX_SERIALIZED_VALUES(h,i,"dummy")
};

struct nonpod3: nonpod_base {
  nonpod3(char h, char i): nonpod_base(h,i) {}
  
  struct upcxx_serialization {
    template<typename Writer>
    static void serialize(Writer &w, nonpod3 const &x) {
      w.write(0xbeef);
      w.write(x.h);
      w.write(x.i);
    }
    template<typename Reader>
    static nonpod3* deserialize(Reader &r, void *spot) {
      UPCXX_ASSERT_ALWAYS(r.template read<int>() == 0xbeef);
      char h = r.template read<char>();
      char i = r.template read<char>();
      return ::new(spot) nonpod3(h,i);
    }
  };
};

struct nonpod4: nonpod_base {
  nonpod4(char h, char i): nonpod_base(h,i) {}
};

namespace upcxx {
  template<>
  struct serialization<nonpod4> {
    // unpspec'd upper-bound support
    template<typename Prefix>
    static auto ubound(Prefix pre, nonpod4 const &x)
      -> decltype(pre.cat_ubound_of(x.h).cat_ubound_of(x.i)) {
      return pre.cat_ubound_of(x.h).cat_ubound_of(x.i);
    }
    
    template<typename Writer>
    static void serialize(Writer &w, nonpod4 const &x) {
      w.write(0xbeef);
      w.write(x.h);
      w.write(x.i);
    }
    template<typename Reader>
    static nonpod4* deserialize(Reader &r, void *spot) {
      UPCXX_ASSERT_ALWAYS(r.template read<int>() == 0xbeef);
      char h = r.template read<char>();
      char i = r.template read<char>();
      return ::new(spot) nonpod4(h,i);
    }
  };
}

static_assert(is_trivially_serializable<const int>::value, "Uh-oh.");

static_assert(is_trivially_serializable<std::pair<int,char>>::value, "Uh-oh.");
static_assert(is_trivially_serializable<std::pair<const int,char>>::value, "Uh-oh.");
static_assert(serialization_traits<std::pair<int,char>>::is_actually_trivially_serializable, "Uh-oh.");
static_assert(serialization_traits<std::pair<const int,char>>::is_actually_trivially_serializable, "Uh-oh.");

static_assert(is_trivially_serializable<std::tuple<int,char>>::value, "Uh-oh.");
static_assert(is_trivially_serializable<std::tuple<const int,char>>::value, "Uh-oh.");
static_assert(serialization_traits<std::tuple<int,char>>::is_actually_trivially_serializable, "Uh-oh.");
static_assert(serialization_traits<std::tuple<const int,char>>::is_actually_trivially_serializable, "Uh-oh.");

static_assert(!is_trivially_serializable<std::string>::value, "Uh-oh");

static_assert(!is_trivially_serializable<nonpod1>::value, "Uh-oh");
static_assert(!is_trivially_serializable<nonpod2>::value, "Uh-oh");
static_assert(!is_trivially_serializable<nonpod3>::value, "Uh-oh");
static_assert(!is_trivially_serializable<nonpod4>::value, "Uh-oh");

int main() {
  print_test_header();
  
  roundtrip<std::int8_t>(1);
  roundtrip<std::uint16_t>(1000);
  roundtrip<std::int32_t>(1<<29);
  roundtrip<std::uint32_t>(1u<<31);
  roundtrip<float>(3.14f);
  roundtrip<double>(3.14);
  roundtrip(nonpod1('h','i'));
  roundtrip(std::array<int,10>{{0,1,2,3,4,5,6,7,8,9}});
  roundtrip<int[10]>({0,1,2,3,4,5,6,7,8,9});
  roundtrip<nonpod2[3]>({{'a','b'}, {'x','y'}, {'u','v'}});
  roundtrip(std::make_pair('a', 1));
  roundtrip(std::make_pair('a', nonpod3('h','i')));
  roundtrip(std::make_tuple('a', 1, 3.14));
  roundtrip(std::make_tuple('a', 1, 3.14, std::string("abcdefghijklmnopqrstuvwxyz")));
  roundtrip(std::vector<int>{1,2,3});
  roundtrip(std::deque<nonpod1>{{'a','b'}, {'x','y'}});
  roundtrip<std::vector<std::list<std::tuple<std::string,nonpod2>>>>(
    std::initializer_list<std::list<std::tuple<std::string,nonpod2>>>{
      {},
      std::initializer_list<std::tuple<std::string,nonpod2>>{
        std::tuple<std::string,nonpod2>("hi",{'a','b'}),
        std::tuple<std::string,nonpod2>("bob",{'x','y'}),
        std::tuple<std::string,nonpod2>("alice",{'\0','!'})
      }
    }
  );
  roundtrip<std::vector<std::list<std::tuple<std::string,nonpod3>>>>(
    std::initializer_list<std::list<std::tuple<std::string,nonpod3>>>{
      {},
      std::initializer_list<std::tuple<std::string,nonpod3>>{
        std::tuple<std::string,nonpod3>("hi",{'a','b'}),
        std::tuple<std::string,nonpod3>("bob",{'x','y'}),
        std::tuple<std::string,nonpod3>("alice",{'\0','!'})
      }
    }
  );
  roundtrip<std::vector<std::list<std::tuple<std::string,nonpod4>>>>(
    std::initializer_list<std::list<std::tuple<std::string,nonpod4>>>{
      {},
      std::initializer_list<std::tuple<std::string,nonpod4>>{
        std::tuple<std::string,nonpod4>("hi",{'a','b'}),
        std::tuple<std::string,nonpod4>("bob",{'x','y'}),
        std::tuple<std::string,nonpod4>("alice",{'\0','!'})
      }
    }
  );

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

  print_test_success();
  return 0;
}
