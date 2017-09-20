#ifndef _f0b217aa_607e_4aa4_8147_82a0d66d6303
#define _f0b217aa_607e_4aa4_8147_82a0d66d6303

#include <upcxx/backend_fwd.hpp>

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

inline std::string test_name(const char *file) {
    const char test_dir[] = "upcxx/test/";
    int pos = std::string{file}.rfind(test_dir);
    pos += sizeof(test_dir)-1; // skip over test_dir substring
    return std::string{file + pos};
}

#ifdef UPCXX_BACKEND
  inline void print_test_header_(const char *file) {
      if(0 == upcxx::rank_me()) {
          std::cout << KLBLUE << "Test: " << test_name(file) << KNORM << std::endl;
          std::cout << KLBLUE << "Ranks: " << upcxx::rank_n() << KNORM << std::endl;
      }
  }

  inline void print_test_success(bool success=true) {
      // include a barrier to ensure all other threads have finished working.
      // flush stdout to prevent any garbling of output
      upcxx::barrier();
      
      if(0 == upcxx::rank_me())
          std::cout << std::flush<< KLGREEN << "Test result: "<<(success?"SUCCESS":"FAILURE") << KNORM << std::endl;
  }
#else
  inline void print_test_header_(const char *file) {
      std::cout << KLBLUE << "Test: " << test_name(file) << KNORM << std::endl;
  }

  inline void print_test_success(bool success=true) {
      std::cout << std::flush<< KLGREEN << "Test result: "<<(success?"SUCCESS":"FAILURE") << KNORM << std::endl;
  }
#endif

#endif
