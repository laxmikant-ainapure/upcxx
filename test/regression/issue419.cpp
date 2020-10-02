#include <upcxx/upcxx.hpp>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <unistd.h>

struct say {
  std::stringstream ss;
  say() {
    *this << "pid:" << (int)getpid() << ": ";
  }
  template<typename T>
  say& operator<<(T const &that) {
    ss << that;
    return *this;
  }
  ~say() {
    *this << "\n";
    std::cout << ss.str() << std::flush;
  }
};

struct A { 
  A() {
     say() << "constructor("<<std::setw(18)<<this<<"): init=" << upcxx::initialized();
  }
  ~A(){ 
     say() << "destructor ("<<std::setw(18)<<this<<"): init=" << upcxx::initialized();
     assert(!upcxx::initialized()); 
  }
};

A a1; // static data

int main() {
  say() << "main()";
  A a2; // stack

  upcxx::init();

  say() << "UPC++ process " << upcxx::rank_me() << "/" << upcxx::rank_n();

  upcxx::barrier();
  if (!upcxx::rank_me()) say() << "SUCCESS";
  upcxx::finalize();
  
  say() << "post-finalize";
  return 0;
}

