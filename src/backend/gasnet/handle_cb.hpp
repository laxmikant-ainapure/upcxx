#ifndef _740290a8_56e6_4fa4_b251_ff87c02bede0
#define _740290a8_56e6_4fa4_b251_ff87c02bede0

#include <cstdint>

namespace upcxx {
namespace backend {
namespace gasnet {
  struct handle_cb {
    handle_cb *next_;
    std::uintptr_t handle;
    
    virtual void execute_and_delete() = 0;
  };
  
  ////////////////////////////////////////////////////////////////////
  
  class handle_cb_queue {
    handle_cb *head_ = nullptr;
    handle_cb **tailp_ = &this->head_;
  
  public:
    handle_cb_queue() = default;
    handle_cb_queue(handle_cb_queue const&) = delete;
    
    void enqueue(handle_cb *cb);
    
    int burst(int burst_n);
  };
  
  inline void handle_cb_queue::enqueue(handle_cb *cb) {
    cb->next_ = nullptr;
    *this->tailp_ = cb;
    this->tailp_ = &cb->next_;
  }
}}}

#endif
