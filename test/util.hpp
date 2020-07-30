#ifndef _f0b217aa_607e_4aa4_8147_82a0d66d6303
#define _f0b217aa_607e_4aa4_8147_82a0d66d6303

#if UPCXX_BACKEND
  #include <upcxx/upcxx.hpp>
#endif

#include <iostream>
#include <string>

#ifdef UPCXX_USE_COLOR
  // These test programs are not smart enough to properly honor termcap
  // Don't issue color codes by default unless specifically requested
  #define KNORM  "\x1B[0m"
  #define KLRED "\x1B[91m"
  #define KLGREEN "\x1B[92m"
  #define KLBLUE "\x1B[94m"
#else
  #define KNORM  ""
  #define KLRED ""
  #define KLGREEN ""
  #define KLBLUE ""
#endif

#define print_test_header() print_test_header_(__FILE__)

template<typename=void>
std::string test_name(const char *file) {
    size_t pos = std::string{file}.rfind("/");
    if (pos == std::string::npos) return std::string(file);
    return std::string{file + pos + 1};
}

#if UPCXX_BACKEND
  template<typename=void>
  void print_test_header_(const char *file) {
      if(!upcxx::initialized() || 0 == upcxx::rank_me()) {
          std::cout << KLBLUE << "Test: " << test_name(file) << KNORM << std::endl;
      }
      if(upcxx::initialized() && 0 == upcxx::rank_me()) {
          std::cout << KLBLUE << "Ranks: " << upcxx::rank_n() << KNORM << std::endl;
      }
  }

  template<typename=void>
  void print_test_success(bool success=true) {
      if(upcxx::initialized()) {
          // include a barrier to ensure all other threads have finished working.
          // flush stdout to prevent any garbling of output
          upcxx::barrier();
      }
      
      std::cout << std::flush<< KLGREEN << "Test result: "<<(success?"SUCCESS":"ERROR") << KNORM << std::endl;
  }
#else
  template<typename=void>
  void print_test_header_(const char *file) {
      std::cout << KLBLUE << "Test: " << test_name(file) << KNORM << std::endl;
  }

  template<typename=void>
  void print_test_success(bool success=true) {
      std::cout << std::flush<< KLGREEN << "Test result: "<<(success?"SUCCESS":"ERROR") << KNORM << std::endl;
  }
#endif

template<typename T1, typename T2>
struct assert_same {
  static_assert(std::is_same<T1, T2>::value, "types differ");
};

#endif
