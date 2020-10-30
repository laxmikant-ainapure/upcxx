#include <sstream>
#include <type_traits>
#include <upcxx/upcxx.hpp>

#include "util.hpp"

using upcxx::global_ptr;
using upcxx::intrank_t;
using upcxx::rank_me;
using upcxx::memory_kind;

struct A {
  double x;
  const int y = 3;
  mutable char z;
  double arr1[1];
  const int arr2[2] = {};
  mutable char arr3[4][3];
};
struct B : A {};

#define CHECK_MEMBEROF_GUTS(accessor, suffix) {                 \
    auto gp_x = accessor(ptr, x) suffix;                        \
    auto gp_y = accessor(ptr, y) suffix;                        \
    auto gp_z = accessor(ptr, z) suffix;                        \
    auto gp_arr1 = accessor(ptr, arr1) suffix;                  \
    auto gp_arr2 = accessor(ptr, arr2) suffix;                  \
    auto gp_arr3 = accessor(ptr, arr3) suffix;                  \
    assert_same<decltype(gp_x), global_ptr<Ex1>>{};             \
    assert_same<decltype(gp_y), global_ptr<Ex2>>{};             \
    assert_same<decltype(gp_z), global_ptr<Ex3>>{};             \
    assert_same<decltype(gp_arr1), global_ptr<Ex1>>{};          \
    assert_same<decltype(gp_arr2), global_ptr<Ex2>>{};          \
    assert_same<decltype(gp_arr3), global_ptr<Ex3>>{};          \
    UPCXX_ASSERT_ALWAYS(gp_x.where() == ptr.where());           \
    UPCXX_ASSERT_ALWAYS(gp_y.where() == ptr.where());           \
    UPCXX_ASSERT_ALWAYS(gp_z.where() == ptr.where());           \
    UPCXX_ASSERT_ALWAYS(gp_arr1.where() == ptr.where());        \
    UPCXX_ASSERT_ALWAYS(gp_arr2.where() == ptr.where());        \
    UPCXX_ASSERT_ALWAYS(gp_arr3.where() == ptr.where());        \
  }

template<typename Ex1, typename Ex2, typename Ex3, typename GP>
static void check_memberof(GP ptr) {
  CHECK_MEMBEROF_GUTS(upcxx_memberof,);
  CHECK_MEMBEROF_GUTS(upcxx_memberof_general, .wait());
}

int main() {
  upcxx::init();

  print_test_header();

  static_assert(std::is_same<global_ptr<int>::element_type,
                             int>::value,
                "unexpected element_type");
  static_assert(std::is_same<global_ptr<const int>::element_type,
                             const int>::value,
                "unexpected element_type");

  assert_same<upcxx::deserialized_type_t<global_ptr<int>>,
              global_ptr<int>>{};
  assert_same<upcxx::deserialized_type_t<global_ptr<const int>>,
              global_ptr<const int>>{};

  global_ptr<int> ptr;
  global_ptr<const int> cptr;
  UPCXX_ASSERT_ALWAYS(ptr.is_null());
  UPCXX_ASSERT_ALWAYS(cptr.is_null());
  UPCXX_ASSERT_ALWAYS(ptr.is_local());
  UPCXX_ASSERT_ALWAYS(cptr.is_local());
  UPCXX_ASSERT_ALWAYS(!ptr);
  UPCXX_ASSERT_ALWAYS(!cptr);

  ptr = upcxx::new_array<int>(10);
  UPCXX_ASSERT_ALWAYS(!ptr.is_null());
  UPCXX_ASSERT_ALWAYS(ptr.is_local());
  UPCXX_ASSERT_ALWAYS(ptr);
  UPCXX_ASSERT_ALWAYS(ptr != cptr);
  UPCXX_ASSERT_ALWAYS(cptr != ptr);

  cptr = ptr;
  UPCXX_ASSERT_ALWAYS(!cptr.is_null());
  UPCXX_ASSERT_ALWAYS(cptr.is_local());
  UPCXX_ASSERT_ALWAYS(cptr);
  UPCXX_ASSERT_ALWAYS(ptr == cptr);
  UPCXX_ASSERT_ALWAYS(cptr == ptr);

  static_assert(std::is_same<decltype(ptr.local()), int*>::value,
                "unexpected return type for local()");
  static_assert(std::is_same<decltype(cptr.local()), const int*>::value,
                "unexpected return type for local()");

  int* lptr = ptr.local();
  const int* lcptr = cptr.local();
  UPCXX_ASSERT_ALWAYS(lptr == lcptr);

  ptr = upcxx::to_global_ptr(lptr);
  cptr = upcxx::to_global_ptr(lcptr);
  UPCXX_ASSERT_ALWAYS(ptr == cptr);

  ptr = upcxx::try_global_ptr(lptr);
  cptr = upcxx::try_global_ptr(lcptr);
  UPCXX_ASSERT_ALWAYS(ptr == cptr);

  UPCXX_ASSERT_ALWAYS(ptr.where() == rank_me());
  UPCXX_ASSERT_ALWAYS(cptr.where() == rank_me());

  ptr = ptr + 3;
  ptr = ptr - 3;
  cptr = cptr + 3;
  cptr = cptr - 3;

  ptr += 3;
  UPCXX_ASSERT_ALWAYS(ptr != cptr);
  UPCXX_ASSERT_ALWAYS(ptr > cptr);
  UPCXX_ASSERT_ALWAYS(ptr >= cptr);
  UPCXX_ASSERT_ALWAYS(cptr < ptr);
  UPCXX_ASSERT_ALWAYS(cptr <= ptr);
  UPCXX_ASSERT_ALWAYS(cptr + 3 == ptr);
  UPCXX_ASSERT_ALWAYS(cptr == ptr - 3);
  UPCXX_ASSERT_ALWAYS(ptr - cptr == 3);
  UPCXX_ASSERT_ALWAYS(cptr - ptr == -3);
  UPCXX_ASSERT_ALWAYS(ptr - (ptr + 1) == -1);
  UPCXX_ASSERT_ALWAYS(cptr - (cptr + 1) == -1);

  ptr -= 3;
  UPCXX_ASSERT_ALWAYS(ptr == cptr);

  ++ptr;
  UPCXX_ASSERT_ALWAYS(ptr - cptr == 1);
  UPCXX_ASSERT_ALWAYS(ptr-- - cptr == 1);
  UPCXX_ASSERT_ALWAYS(ptr == cptr);
  ++cptr;
  cptr--;
  --ptr;
  ptr++;
  UPCXX_ASSERT_ALWAYS(ptr == cptr);

  std::stringstream ss1, ss2;
  ss1 << ptr;
  ss2 << ptr;
  UPCXX_ASSERT_ALWAYS(ss1.str() == ss2.str());

  global_ptr<unsigned int> uptr =
    upcxx::reinterpret_pointer_cast<unsigned int>(ptr);
  ptr = upcxx::reinterpret_pointer_cast<int>(uptr);
  UPCXX_ASSERT_ALWAYS(ptr == cptr);
  ptr = upcxx::const_pointer_cast<int>(cptr);
  UPCXX_ASSERT_ALWAYS(ptr == cptr);

  global_ptr<const unsigned int> ucptr =
    upcxx::reinterpret_pointer_cast<const unsigned int>(cptr);
  cptr = upcxx::reinterpret_pointer_cast<const int>(ucptr);
  UPCXX_ASSERT_ALWAYS(ptr == cptr);
  cptr = upcxx::const_pointer_cast<const int>(ptr);
  UPCXX_ASSERT_ALWAYS(ptr == cptr);

  global_ptr<A> base_ptr;
  global_ptr<B> derived_ptr1 = upcxx::new_<B>();
  base_ptr = upcxx::static_pointer_cast<A>(derived_ptr1);
  global_ptr<B> derived_ptr2 =
    upcxx::static_pointer_cast<B>(base_ptr);
  UPCXX_ASSERT_ALWAYS(!base_ptr.is_null());
  UPCXX_ASSERT_ALWAYS(derived_ptr1 == derived_ptr2);

  global_ptr<const A> base_cptr;
  global_ptr<const B> derived_cptr1 = derived_ptr1;
  base_cptr = upcxx::static_pointer_cast<const A>(derived_cptr1);
  global_ptr<const B> derived_cptr2 =
    upcxx::static_pointer_cast<const B>(base_cptr);
  UPCXX_ASSERT_ALWAYS(!base_cptr.is_null());
  UPCXX_ASSERT_ALWAYS(derived_cptr1 == derived_cptr2);

  check_memberof<double, const int, char>(base_ptr);
  check_memberof<const double, const int, char>(base_cptr);

  upcxx::delete_(derived_ptr1);

  global_ptr<int, memory_kind::any> aptr = ptr;
  ptr = upcxx::static_kind_cast<memory_kind::host>(aptr);
  ptr = upcxx::dynamic_kind_cast<memory_kind::host>(aptr);

  global_ptr<const int, memory_kind::any> captr = cptr;
  cptr = upcxx::static_kind_cast<memory_kind::host>(captr);
  cptr = upcxx::dynamic_kind_cast<memory_kind::host>(captr);

  UPCXX_ASSERT_ALWAYS(ptr == cptr);

  UPCXX_ASSERT_ALWAYS(std::less<global_ptr<const int>>()(ptr - 1, cptr));
  UPCXX_ASSERT_ALWAYS(std::less_equal<global_ptr<const int>>()(ptr - 1, cptr));
  UPCXX_ASSERT_ALWAYS(std::greater<global_ptr<const int>>()(ptr + 1, cptr));
  UPCXX_ASSERT_ALWAYS(std::greater_equal<global_ptr<const int>>()(ptr + 1, cptr));

  UPCXX_ASSERT_ALWAYS(std::less<global_ptr<int>>()(ptr - 1, ptr));
  UPCXX_ASSERT_ALWAYS(std::less_equal<global_ptr<int>>()(ptr - 1, ptr));
  UPCXX_ASSERT_ALWAYS(std::greater<global_ptr<int>>()(ptr + 1, ptr));
  UPCXX_ASSERT_ALWAYS(std::greater_equal<global_ptr<int>>()(ptr + 1, ptr));

  UPCXX_ASSERT_ALWAYS(std::hash<global_ptr<int>>()(ptr) ==
                      std::hash<global_ptr<const int>>()(cptr));

  upcxx::delete_array(ptr);

  print_test_success();

  upcxx::finalize();
  return 0;
}
