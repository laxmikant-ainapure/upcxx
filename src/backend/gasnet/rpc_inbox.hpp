#ifndef _432114f2_8d13_4ea7_8d83_acbfd4852979
#define _432114f2_8d13_4ea7_8d83_acbfd4852979

#include <upcxx/diagnostic.hpp>
#include <upcxx/command.hpp>

#include <cstdint>
#include <cstring>
#include <cstdlib>

namespace upcxx {
namespace backend {
namespace gasnet {
  struct rpc_message {
    rpc_message *next_ = this;
    void *payload;
    
    // Build copy of a packed command buffer (upcxx/command.hpp) as
    // a rpc_message.
    static rpc_message* build_copy(
      void *cmd_buf,
      std::size_t cmd_size,
      std::size_t cmd_alignment // alignment requirement of packing
    );
    
    void execute_and_delete();
  };
  
  class rpc_inbox {
    rpc_message *head_ = nullptr;
    rpc_message **tailp_ = &this->head_;
    
  public:
    rpc_inbox() = default;
    rpc_inbox(rpc_inbox const&) = delete;
    
    void enqueue(rpc_message *m);
    
    int burst(int burst_n = 100);
  };
  
  //////////////////////////////////////////////////////////////////////
  
  inline rpc_message* rpc_message::build_copy(
      void *cmd_buf,
      std::size_t cmd_size,
      std::size_t cmd_alignment
    ) {
    
    size_t msg_size = cmd_size;
    msg_size = (msg_size + alignof(rpc_message)-1) & -alignof(rpc_message);
    
    size_t msg_offset = msg_size;
    msg_size += sizeof(rpc_message);
    
    void *msg_buf;
    int ok = posix_memalign(&msg_buf, cmd_alignment, msg_size);
    UPCXX_ASSERT_ALWAYS(ok == 0);
    
    rpc_message *m = new((char*)msg_buf + msg_offset) rpc_message;
    m->payload = msg_buf;
    
    // The (void**) casts *might* inform memcpy that it can assume word
    // alignment.
    std::memcpy((void**)msg_buf, (void**)cmd_buf, cmd_size);
    
    return m;
  }
  
  inline void rpc_message::execute_and_delete() {
    UPCXX_ASSERT(this->next_ == this); // cannot be in an rpc_inbox
    
    upcxx::parcel_reader r{this->payload};
    future<> buf_done = upcxx::command_execute(r);
    
    // delete buffer when future says its ok
    buf_done >> [this]() {
      std::free(this->payload);
    };
  }
  
  inline void rpc_inbox::enqueue(rpc_message *m) {
    // add to list
    m->next_ = nullptr;
    *this->tailp_ = m;
    this->tailp_ = &m->next_;
  }
}}}
#endif
