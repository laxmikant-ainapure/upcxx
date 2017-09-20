#include <upcxx/backend/gasnet/handle_cb.hpp>

#include <gasnet.h>

using upcxx::backend::gasnet::handle_cb_queue;

int handle_cb_queue::burst(int burst_n) {
  int exec_n = 0;
  handle_cb **pp = &this->head_;
  
  while(burst_n-- && *pp != nullptr) {
    handle_cb *p = *pp;
    gasnet_handle_t handle = reinterpret_cast<gasnet_handle_t>(p->handle);
    
    if(GASNET_OK == gasnet_try_syncnb(handle)) {
      // remove from queue
      *pp = p->next_;
      if(*pp == nullptr)
        this->tailp_ = pp;
      
      // do it!
      p->execute_and_delete();
      exec_n += 1;
    }
    else
      pp = &p->next_;
  }
  
  return exec_n;
}
