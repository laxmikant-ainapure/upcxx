#include <upcxx/backend/gasnet/rpc_inbox.hpp>
#include <upcxx/command.hpp>

using upcxx::backend::gasnet::rpc_inbox;
using upcxx::future;
using upcxx::parcel_reader;

int rpc_inbox::burst(int burst_n) {
  int exec_n = 0;
  rpc_message *m = this->head_;
  
  while(exec_n != burst_n && m != nullptr) {
    // execute rpc
    parcel_reader r{m->payload};
    future<> buf_done = command_execute(r);
    
    rpc_message *next = m->next_;
    
    // delete buffer when future says its ok
    buf_done >> [=]() {
      std::free(m->payload);
    };
    
    exec_n += 1;
    m = next;
  }
  
  this->head_ = m;
  if(m == nullptr)
    this->tailp_ = &this->head_;
  
  return exec_n;
}
