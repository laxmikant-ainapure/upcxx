#include <upcxx/lpc/inbox_lockfree.hpp>

using upcxx::lpc_inbox_lockfree;

template<int queue_n>
int lpc_inbox_lockfree<queue_n>::burst(int q, int burst_n) {
  using lpc = lpc_inbox_lockfree::lpc;

  int exec_n = 0;
  lpc *p = this->head_[q].load(std::memory_order_relaxed);
  
  if(p == nullptr)
    return 0;
  // So there's at least one element in the list...

  // Execute as many elements as we can until we reach one that looks
  // like it may be the last in the list.
  while(true) {
    lpc *next = p->next.load(std::memory_order_relaxed);
    if(next == nullptr)
      break; // Element has no `next`, so it looks like the last.
    
    p->execute_and_delete();
    p = next;
    
    if(burst_n == ++exec_n) {
      this->head_[q].store(p, std::memory_order_relaxed);
      return exec_n;
    }
  }
  
  // We executed as many as we could without doing an `exchange`.
  // If that was at least one then we quit and hope next time we burst
  // there will be even more so we can kick the can of doing the heavy
  // atomic once again.
  if(exec_n != 0) {
    this->head_[q].store(p, std::memory_order_relaxed);
    return exec_n;
  }
  
  // So it *looks* like there is exactly one element in the list (though
  // more may be on the way as we speak). Since it isn't safe to execute
  // an element without knowing its successor first (thanks to
  // execute_and_*DELETE*), we reset the list to start at our `head_`
  // pointer. The reset is done with an `exchange`, with the effects:
  //  1) The last element present in the list before reset is returned
  //     to us (actually the address of its `next` field is).
  //  2) All elements added after reset will start at `head_` and so
  //     won't be successors of the last element from 1.
  this->head_[q].store(nullptr, std::memory_order_relaxed);
  std::atomic<lpc*> *last_next = this->tailp_[q].exchange(&this->head_[q]);
  
  // Process all elements before the last.
  while(&p->next != last_next) {
    // Get next pointer, and must spin for it. Spin should be of
    // extremely short duration since we know that it's on the way by
    // virtue of this not being the tail element.
    lpc *next = p->next.load(std::memory_order_relaxed);
    while(next == nullptr) {
      // TODO: add pause instruction here
      // asm volatile("pause\n": : :"memory");
      next = p->next.load(std::memory_order_relaxed);
    }
    
    p->execute_and_delete();
    p = next;

    // We have no choice but to ignore the `burst_n` budget since we
    // are the only ones who know these elements exist (unless we kept
    // a pointer in our datastructure to stash these elements for
    // consumption in a later burst). Also, it is unlikely that we
    // would blow our budget by much since this list remnant is
    // probably length 1.
    exec_n += 1;
  }

  // And now the last.
  p->execute_and_delete();
  exec_n += 1;

  return exec_n;
}

template int lpc_inbox_lockfree<1>::burst(int, int);
template int lpc_inbox_lockfree<2>::burst(int, int);
