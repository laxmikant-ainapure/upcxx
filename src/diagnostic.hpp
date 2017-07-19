#ifndef _7949681d_8a89_4f83_afb9_de702bf1a46b
#define _7949681d_8a89_4f83_afb9_de702bf1a46b

#include <sstream>

namespace upcxx {
  void dbgbrk();
  void assert_failed(const char *file, int line, const char *msg=nullptr);
}

#define UPCXX_ASSERT_1(ok) \
  do { \
    if(!(ok)) \
      ::upcxx::assert_failed(__FILE__, __LINE__); \
  } while(0);

#define UPCXX_ASSERT_2(ok, ios_msg) \
  do { \
    if(!(ok)) { \
      ::std::stringstream ss; \
      ss << ios_msg; \
      ::upcxx::assert_failed(__FILE__, __LINE__, ss.str().c_str()); \
    } \
  } while(0);

#define UPCXX_ASSERT_DISPATCH(_1, _2, NAME, ...) NAME
#define UPCXX_ASSERT(...) UPCXX_ASSERT_DISPATCH(__VA_ARGS__, UPCXX_ASSERT_2, UPCXX_ASSERT_1)(__VA_ARGS__)

#define UPCXX_FAIL() ::upcxx::assert_failed(__FILE__, __LINE__)

#define UPCXX_VERIFY(ok) \
  do { \
    if(!(ok)) \
      ::upcxx::assert_failed(__FILE__, __LINE__); \
  } while(0);

#endif
