#include <sstream>
#include <upcxx/allocate.hpp>
#include <upcxx/global_ptr.hpp>

#include "util.hpp"

using upcxx::global_ptr;
using upcxx::intrank_t;
using upcxx::rank_me;
using upcxx::memory_kind;

struct A {};
struct B : A {};

int main() {
  upcxx::init();

  print_test_header();

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
  ptr = upcxx::reinterpret_pointer_cast<int>(ptr);
  UPCXX_ASSERT_ALWAYS(ptr == cptr);
  ptr = upcxx::const_pointer_cast<int>(cptr);
  UPCXX_ASSERT_ALWAYS(ptr == cptr);

  global_ptr<const unsigned int> ucptr =
    upcxx::reinterpret_pointer_cast<const unsigned int>(cptr);
  cptr = upcxx::reinterpret_pointer_cast<const int>(cptr);
  UPCXX_ASSERT_ALWAYS(ptr == cptr);
  cptr = upcxx::const_pointer_cast<const int>(ptr);
  UPCXX_ASSERT_ALWAYS(ptr == cptr);

  global_ptr<A> base_ptr;
  global_ptr<B> derived_ptr =
    upcxx::static_pointer_cast<B>(base_ptr);
  base_ptr = upcxx::static_pointer_cast<A>(derived_ptr);
  UPCXX_ASSERT_ALWAYS(base_ptr.is_null());

  global_ptr<const A> base_cptr;
  global_ptr<const B> derived_cptr =
    upcxx::static_pointer_cast<const B>(base_cptr);
  base_cptr = upcxx::static_pointer_cast<const A>(derived_cptr);
  UPCXX_ASSERT_ALWAYS(base_cptr.is_null());

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
