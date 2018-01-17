#include <upcxx/backend/gasnet/handle_cb.hpp>

#include <gasnetex.h>

using upcxx::backend::gasnet::handle_cb_queue;

static_assert(
  sizeof(std::uintptr_t) == sizeof(gex_Event_t),
  "Failed: sizeof(std::uintptr_t) == sizeof(gex_Event_t)"
);

int handle_cb_queue::burst(int burst_n) {
  int exec_n = 0;
  handle_cb **pp = &this->head_;
  
  while(burst_n-- && *pp != nullptr) {
    handle_cb *p = *pp;
    gex_Event_t ev = reinterpret_cast<gex_Event_t>(p->handle);
    
    if(0 == gex_Event_Test(ev)) {
      // remove from queue
      *pp = p->next_;
      if(*pp == nullptr)
        this->tailp_ = pp;
      
      // do it!
      p->execute_and_delete(handle_cb_successor{this, pp});
      
      exec_n += 1;
    }
    else
      pp = &p->next_;
  }
  
  return exec_n;
}
