#include <upcxx/lpc/inbox_syncfree.hpp>

using upcxx::lpc_inbox_syncfree;

template<int queue_n>
int lpc_inbox_syncfree<queue_n>::burst(int q, int burst_n) {
  using lpc = lpc_inbox_syncfree::lpc;
  
  lpc *got_head;
  lpc **got_tailp;
  int exec_n = 0;
  
  { // steal the current list into `got`, replace with empty list
    got_head = this->head_[q];
    got_tailp = this->get_tailp(q);
    
    this->head_[q] = nullptr;
    this->set_tailp(q, &this->head_[q]);
  }
  
  // process stolen list
  while(exec_n != burst_n && got_head != nullptr) {
    lpc *next = got_head->next;
    got_head->execute_and_delete();
    got_head = next;
    exec_n += 1;
  }
  
  // prepend remainder of unexecuted stolen list back into main list
  if(got_head != nullptr) {
    if(this->head_[q] == nullptr)
      this->set_tailp(q, got_tailp);
    
    *got_tailp = this->head_[q];
    this->head_[q] = got_head;
  }
  
  return exec_n;
}

template int lpc_inbox_syncfree<1>::burst(int, int);
template int lpc_inbox_syncfree<2>::burst(int, int);
