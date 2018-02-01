#ifndef _740290a8_56e6_4fa4_b251_ff87c02bede0
#define _740290a8_56e6_4fa4_b251_ff87c02bede0

#include <upcxx/diagnostic.hpp>

#include <cstdint>

namespace upcxx {
namespace backend {
namespace gasnet {
  struct handle_cb;
  struct handle_cb_queue;
  
  struct handle_cb_successor {
    handle_cb_queue *q_;
    handle_cb **pp_;
    void operator()(handle_cb *succ);
  };
  
  struct handle_cb {
    handle_cb *next_ = reinterpret_cast<handle_cb*>(0x1);
    std::uintptr_t handle = 0;
    
    virtual void execute_and_delete(handle_cb_successor) = 0;
  };

  struct handle_cb_queue {
    handle_cb *head_ = nullptr;
    handle_cb **tailp_ = &this->head_;
  
  public:
    handle_cb_queue() = default;
    handle_cb_queue(handle_cb_queue const&) = delete;
    
    void enqueue(handle_cb *cb);
    
    int burst(int burst_n);
  };
  
  ////////////////////////////////////////////////////////////////////
  
  inline void handle_cb_queue::enqueue(handle_cb *cb) {
    UPCXX_ASSERT(cb->next_ == reinterpret_cast<handle_cb*>(0x1));
    cb->next_ = nullptr;
    *this->tailp_ = cb;
    this->tailp_ = &cb->next_;
  }

  inline void handle_cb_successor::operator()(handle_cb *succ) {
    if(succ->next_ == reinterpret_cast<handle_cb*>(0x1)) {
      if(*pp_ == nullptr)
        q_->tailp_ = &succ->next_;
      succ->next_ = *pp_;
      *pp_ = succ;
    }
  }
}}}

#endif
