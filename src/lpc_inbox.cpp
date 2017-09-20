#include <upcxx/lpc_inbox.hpp>
#include <upcxx/diagnostic.hpp>

template<int queue_n, bool thread_safe>
int upcxx::lpc_inbox<queue_n, thread_safe>::burst(int q, int burst_n) {
  using lpc = typename lpc_inbox_base1<queue_n>::lpc;
  
  // Technically, this->head_ should have type atomic<message*> and we
  // should be modifying it will all load/store memory_order_relaxed,
  // but due to lazyness that isn't happening. We offer this pitiful
  // assertion instead.
  static_assert(
    sizeof(lpc*) == sizeof(std::atomic<lpc*>),
    "Pointers aren't atomicly read/writeable!"
  );
  
  // don't bother with locks if there ain't nothing up there
  if(this->head_[q] == nullptr)
    return 0;
  
  lpc *got_head;
  lpc **got_tailp;
  int exec_n = 0;
  
  { // steal the current list into `got`, replace with empty list
    auto locked = this->locked_scope();
    
    got_head = this->head_[q];
    got_tailp = this->tailp_[q];
    
    this->head_[q] = nullptr;
    this->tailp_[q] = &this->head_[q];
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
    auto locked = this->locked_scope();
    
    if(this->head_[q] == nullptr)
      this->tailp_[q] = got_tailp;
    
    *got_tailp = this->head_[q];
    this->head_[q] = got_head;
  }
  
  return exec_n;
}

template int upcxx::lpc_inbox</*queue_n=*/1, /*thread_safe=*/true>::burst(int, int);
template int upcxx::lpc_inbox</*queue_n=*/1, /*thread_safe=*/false>::burst(int, int);
template int upcxx::lpc_inbox</*queue_n=*/2, /*thread_safe=*/true>::burst(int, int);
template int upcxx::lpc_inbox</*queue_n=*/2, /*thread_safe=*/false>::burst(int, int);
