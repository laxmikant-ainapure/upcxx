#include <upcxx/backend/gasnet/rpc_inbox.hpp>
#include <upcxx/command.hpp>

using upcxx::backend::gasnet::rpc_inbox;
using upcxx::parcel_reader;

int rpc_inbox::burst(int burst_n) {
  int exec_n = 0;
  
  while(exec_n != burst_n && this->head_ != nullptr) {
    rpc_message *m = this->head_;
    rpc_message *next = m->next_;

    // remove head
    this->head_ = next;
    if(next == nullptr)
      this->tailp_ = &this->head_;
    
    // execute rpc
    parcel_reader r{m->payload};
    command<bool,void*>::execute(r, /*use_free=*/true, /*buf=*/m->payload);

    exec_n += 1;
  }
  
  return exec_n;
}
