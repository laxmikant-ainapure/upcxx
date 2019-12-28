#include <stddef.h>
#include <type_traits>
#include <iostream>
#include <cassert>
#include <upcxx/upcxx.hpp>

#define T(t) if (!upcxx::rank_me()) do { \
    std::cout << " *** " << #t << " *** " << std::endl; \
    std::cout << "std::is_standard_layout<" #t "> = " \
              << std::is_standard_layout<t>::value << std::endl; \
    std::cout << "std::is_trivial<" #t "> = " \
              << std::is_trivial<t>::value << std::endl; \
    std::cout << "std::is_pod<" #t "> = " \
              << std::is_pod<t>::value << std::endl; \
  } while (0)


struct A {  // standard layout, trivial, POD
  char f0;
  char f1;
  char f2;
  double x;
};

struct B {  // standard layout, NOT trivial, NOT POD
  char f0;
  char f1;
  char f2;
  double x;
  B() {}
};

struct C {  // NOT standard layout, trivial, NOT POD
  char f0;
  char f1;
  char f2;
  private:
  double x;
};

struct D {  // NOT standard layout, NOT trivial, NOT POD
  char f0;
  char f1;
  char f2;
  double x;
  char &f;
  D() : f(f0) {}
};

template<typename T, bool stdlayout>
struct calc { static void _(upcxx::global_ptr<T> gp_o) {
  if (!upcxx::rank_me()) std::cout << "Testing standard layout..." << std::endl;
  upcxx::global_ptr<char> gp_f0 = upcxx_memberof(gp_o, f0);
  upcxx::global_ptr<char> gp_f1 = upcxx_memberof(gp_o, f1);
  upcxx::global_ptr<char> gp_f2 = upcxx_memberof(gp_o, f2);
  assert(gp_f0 && gp_f1 && gp_f2);
  upcxx::global_ptr<char> gp_base = upcxx::reinterpret_pointer_cast<char>(gp_o);
  ssize_t d0 = gp_f0 - gp_base;
  ssize_t d1 = gp_f1 - gp_base;
  ssize_t d2 = gp_f2 - gp_base;
  if (!upcxx::rank_me()) {
    std::cout << "memberof offsets: d0=" << d0 << " d1=" << d1 << " d2=" << d2 << std::endl;
  }
  upcxx::barrier();
  assert(d0 == 0 && d1 == 1 && d2 == 2);
  upcxx::barrier();

  #if 0
  // test some non-trivial expressions
  volatile int zero = 0;

  upcxx::global_ptr<char> e1_test = upcxx_memberof(gp_o+zero, f1);
  assert(e1_test == gp_f1);

  upcxx::global_ptr<char> e2_test = upcxx_memberof((gp_o), f1);
  assert(e2_test == gp_f1);

  using gp_T_any = upcxx::global_ptr<T, upcxx::memory_kind::any>;
  using gp_F_any = upcxx::global_ptr<char, upcxx::memory_kind::any>;
  gp_T_any gp_o_any = gp_o;
  assert(upcxx_memberof((gp_o_any + zero), f0).kind == upcxx::memory_kind::any);
  gp_F_any e3_test = upcxx_memberof((gp_o_any + zero), f1);
  assert(e3_test == gp_f1);

  upcxx::global_ptr<T> const gp_o_c = gp_o; 
  upcxx::global_ptr<char> e4_test = upcxx_memberof(gp_o_c, f1);
  assert(e4_test == gp_f1);
  upcxx::barrier();
  #endif

  #if 0
  int se = 0; // test for single-evaluation
  auto func = [&](){se++; return gp_o;};
  upcxx::global_ptr<char> se_test = upcxx_memberof(func(), f0);
  assert(se == 1);
  upcxx::barrier();
  #endif
} };
template<typename T>
struct calc<T,false>{ static void _(upcxx::global_ptr<T> gp_o){
  if (!upcxx::rank_me()) std::cout << "Testing non-standard layout..." << std::endl;
  upcxx::global_ptr<char> gp_f0 = upcxx_memberof_unsafe(gp_o, f0);
  upcxx::global_ptr<char> gp_f1 = upcxx_memberof_unsafe(gp_o, f1);
  upcxx::global_ptr<char> gp_f2 = upcxx_memberof_unsafe(gp_o, f2);
  assert(gp_f0 && gp_f1 && gp_f2);
  upcxx::global_ptr<char> gp_base = upcxx::reinterpret_pointer_cast<char>(gp_o);
  ssize_t d0 = gp_f0 - gp_base;
  ssize_t d1 = gp_f1 - gp_base;
  ssize_t d2 = gp_f2 - gp_base;
  if (!upcxx::rank_me()) {
    std::cout << "memberof_unsafe offsets: d0=" << d0 << " d1=" << d1 << " d2=" << d2 << std::endl;
  }
  upcxx::barrier();
  assert(d0 == 0 && d1 == 1 && d2 == 2);
  upcxx::barrier();
} };

template<typename T>
void check() {
  upcxx::barrier();
  constexpr bool stdlayout = std::is_standard_layout<T>::value;
  T myo;
  ssize_t d1 = uintptr_t(&myo.f0) - uintptr_t(&myo);
  ssize_t d2 = uintptr_t(&myo.f1) - uintptr_t(&myo);
  ssize_t d3 = uintptr_t(&myo.f2) - uintptr_t(&myo);
  ssize_t o1=0,o2=0,o3=0;
  o1 = offsetof(T, f0);
  o2 = offsetof(T, f1);
  o3 = offsetof(T, f2);
  if (!upcxx::rank_me()) {
    std::cout << "delta f0=" << d1 << " offsetof(f0)=" << o1 << std::endl;
    std::cout << "delta f1=" << d2 << " offsetof(f1)=" << o2 << std::endl;
    std::cout << "delta f2=" << d3 << " offsetof(f2)=" << o3 << std::endl;
    if (stdlayout) {
      if (o1 != d1 || o2 != d2 || o3 != d3)
        std::cerr << "ERROR: delta/offsetof mismatch" << std::endl;
      if (o1 != 0)
        std::cerr << "ERROR: first field of standard layout class is not pointer-interconvertible (see C++ [basic.compound])" << std::endl;
    } else {
      if (o1 != d1 || o2 != d2 || o3 != d3)
        std::cerr << "WARNING: delta/offsetof mismatch (non-standard-layout)" << std::endl;
    }
  }
  upcxx::barrier();

  upcxx::global_ptr<T> gp_o;
  if (!upcxx::rank_me()) gp_o = upcxx::new_array<T>(10);
  gp_o = upcxx::broadcast(gp_o, 0).wait();
  assert(gp_o);
  calc<T, stdlayout>::_( gp_o );

  upcxx::barrier();
}

int main() {
  upcxx::init();

  T(A); 
  check<A>();
  T(B); 
  check<B>();
  T(C); 
  check<C>();
  T(D); 
  check<D>();

  upcxx::barrier();
  if (!upcxx::rank_me()) std::cout << "SUCCESS" << std::endl;
  upcxx::finalize();
}
