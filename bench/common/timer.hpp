#ifndef _272a6afc_0487_47e1_a0e1_1f90d04b2e1b
#define _272a6afc_0487_47e1_a0e1_1f90d04b2e1b

#include <chrono>

namespace bench {
  class timer {
    using time_point = std::chrono::steady_clock::time_point;
    time_point t0;
  public:
    timer() {
      t0 = std::chrono::steady_clock::now();
    }
    
    double elapsed() const {
      time_point t1 = std::chrono::steady_clock::now();
      return std::chrono::duration<double>(t1 - t0).count();
    }
    
    double reset() {
      time_point t1 = std::chrono::steady_clock::now();
      double ans = std::chrono::duration<double>(t1 - t0).count();
      t0 = t1;
      return ans;
    }
  };
}
#endif
