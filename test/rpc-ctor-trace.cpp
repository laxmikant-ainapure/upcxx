#include <upcxx/upcxx.hpp>
#include "util.hpp"

struct T {
  static int ctors, dtors, copies, moves;
  static void show_stats(char const *title, int expected_ctors, int expected_copies);
  
  T() { ctors++; }
  T(T const &that) {
    copies++;
  }
  T(T &&that) { moves++; }
  ~T() { dtors++; }

  UPCXX_SERIALIZED_FIELDS()
};

int T::ctors = 0;
int T::dtors = 0;
int T::copies = 0;
int T::moves = 0;

void T::show_stats(const char *title, int expected_ctors, int expected_copies) {
  upcxx::barrier();
  
  #if 1
  UPCXX_ASSERT_ALWAYS(ctors == expected_ctors, title<<": ctors="<<ctors<<" expected="<<expected_ctors);
  UPCXX_ASSERT_ALWAYS(copies == expected_copies, title<<": copies="<<ctors<<" expected="<<expected_copies);
  UPCXX_ASSERT_ALWAYS(ctors+copies+moves == dtors, title<<": ctors - dtors != 0");
  #endif
  
  if(upcxx::rank_me() == 0) {
    std::cout<<title<<std::endl;
    std::cout<<"  T::ctors = "<<ctors<<std::endl;
    std::cout<<"  T::copies = "<<copies<<std::endl;
    std::cout<<"  T::moves = "<<moves<<std::endl;
    std::cout<<"  T::dtors = "<<dtors<<std::endl;
    std::cout<<std::endl;
  }

  ctors = copies = moves = dtors = 0;
}

int main() {
  upcxx::init();
  print_test_header();

  using upcxx::dist_object;
  dist_object<int> dob(3);;

  int target = (upcxx::rank_me() + 1) % upcxx::rank_n();

  // About the expected num of copies: unfortunately the future<T> returning cases
  // involve extra copy due to backend::send_awaken_lpc requiring std::tuple<T>, but
  // the future<T> is a heap shared type so it isnt safe for the runtime to move
  // the T out to the tuple. In the future send_awaken_lpc should be changed
  // to take a reference to T and serialize from that place.
  
  upcxx::rpc(target,
    [](T &&x) -> T {
      return std::move(x);
    },
    T()
  ).wait_reference();
  T::show_stats("T&& -> T", 3, 0);

  upcxx::rpc(target,
    [](T const &x) -> T {
      return x;
    },
    T()
  ).wait_reference();
  T::show_stats("T const& -> T", 3, 1);

  upcxx::rpc(target,
    [](T &&x) -> upcxx::future<T> {
      return upcxx::make_future(std::move(x));
    },
    T()
  ).wait_reference();
  T::show_stats("T&& -> future<T>", 3, 1);

  upcxx::rpc(target,
    [](T const &x) -> upcxx::future<T> {
      return upcxx::make_future(x);
    },
    T()
  ).wait_reference();
  T::show_stats("T const& -> future<T>", 3, 2);

  // now with dist_object

  upcxx::rpc(target,
    [](dist_object<int>&, T &&x) -> T {
      return std::move(x);
    },
    dob, T()
  ).wait_reference();
  T::show_stats("dist_object + T&& -> T", 3, 0);

  upcxx::rpc(target,
    [](dist_object<int>&, T const &x) -> T {
      return x;
    },
    dob, T()
  ).wait_reference();
  T::show_stats("dist_object + T const& -> T", 3, 1);

  upcxx::rpc(target,
    [](dist_object<int>&, T &&x) -> upcxx::future<T> {
      return upcxx::make_future(std::move(x));
    },
    dob, T()
  ).wait_reference();
  T::show_stats("dist_object + T&& -> future<T>", 3, 1);

  upcxx::rpc(target,
    [](dist_object<int>&, T const &x) -> upcxx::future<T> {
      return upcxx::make_future(x);
    },
    dob, T()
  ).wait_reference();
  T::show_stats("dist_object + T const& -> future<T>", 3, 2);

  print_test_success();
  upcxx::finalize();
}
