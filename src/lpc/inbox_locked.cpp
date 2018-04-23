#include <upcxx/lpc/inbox_locked.hpp>

#include <atomic>

using upcxx::lpc_inbox_locked;

std::mutex upcxx::detail::lpc_inbox_locked_base::the_locks[1<<lock_log2_n];

template<int queue_n>
int lpc_inbox_locked<queue_n>::burst(int q, int burst_n) {
  using lpc = lpc_inbox_locked::lpc;
  
  // Technically, this->head_ should have type atomic<lpc*> and we
  // should be modifying it with all load/store memory_order_relaxed,
  // but due to lazyness that isn't happening. We offer this pitiful
  // assertion instead.
  static_assert(
    sizeof(lpc*) == sizeof(std::atomic<lpc*>),
    "Pointers aren't atomically read/writeable!"
  );
  
  // Don't bother with locks if nothing to do.
  if(this->head_[q] == nullptr)
    return 0;
  
  lpc *got_head;
  lpc *got_tail;
  int exec_n = 0;
  
  std::mutex &lock = this->get_lock();
  
  // Steal the current list into `got`, replace with empty list.
  {
    std::lock_guard<std::mutex> locked(lock);
  
    got_head = this->head_[q];
    got_tail = this->tail_[q];
    
    this->head_[q] = nullptr;
    this->tail_[q] = nullptr;
  }
  
  // Process stolen list.
  while(exec_n != burst_n && got_head != nullptr) {
    lpc *next = got_head->next;
    got_head->execute_and_delete();
    got_head = next;
    exec_n += 1;
  }
  
  // Prepend remainder of unexecuted stolen list back into main list.
  if(got_head != nullptr) {
    std::lock_guard<std::mutex> locked(lock);
    
    if(this->head_[q] != nullptr)
      got_tail->next = this->head_[q];
    else
      this->tail_[q] = got_tail;
    
    this->head_[q] = got_head;
  }
  
  return exec_n;
}

template int lpc_inbox_locked<1>::burst(int, int);
template int lpc_inbox_locked<2>::burst(int, int);
