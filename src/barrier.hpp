#ifndef _f2c7c1fc_cd4a_4123_bf50_a6542f8efa2c
#define _f2c7c1fc_cd4a_4123_bf50_a6542f8efa2c

#include <upcxx/backend.hpp>
#include <upcxx/completion.hpp>
#include <upcxx/future.hpp>
#include <upcxx/team.hpp>

namespace upcxx {
  void barrier(team &tm = upcxx::world());
  
  template<typename Cxs = completions<future_cx<operation_cx_event>>>
  future<> barrier_async(team &tm = upcxx::world(), Cxs cxs_ignored = Cxs{{}});
}
#endif
