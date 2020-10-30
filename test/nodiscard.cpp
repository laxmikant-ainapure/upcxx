#include <upcxx/upcxx.hpp>
#include <iostream>

using namespace upcxx;

bool false_val() { static volatile bool fval = false; return fval; }

int main() {
  upcxx::init();
  
  std::cout<<"ERROR: This test is compile-only and not meant to be run."<<std::endl;

  if (false_val()) {
  #define CONCAT2(x,y) x ## y
  #define CONCAT(x,y) CONCAT2(x,y)
  #define VAR CONCAT(var,__LINE__)
  promise<int> p,p2;
  future<> f;
  size_t sz = 1024;
  global_ptr<int> gp = new_array<int>(sz);
  dist_object<global_ptr<int>> dobj(gp);
  global_ptr<int> gpo = dobj.fetch(0).wait();
  int *lp = new int[sz];
  atomic_domain<int> ad({ atomic_op::load, atomic_op::store, atomic_op::compare_exchange, 
                          atomic_op::inc, atomic_op::fetch_inc,
                          atomic_op::add, atomic_op::fetch_add });
  // ---------------------------------------------------------------------------------
  // none of the following should warn

  // *** Direct future operations that drop a future
  
  // We don't have enough static info to guarantee these lead to an incorrect program
  // Some of them are obviously pointless/wasteful, but we don't warn about that
  make_future(1);       // probably a (harmless) mistake
  to_future(1);         // probably a (harmless) mistake
  when_all(f);          // probably a (harmless) mistake
  f.result();           // probably a (harmless) mistake
  f.result_reference(); // probably a (harmless) mistake
  f.wait();       // dropping a wait return is idiomatic for empty futures, and other reasonable cases
  f.then([]{});   // dropping a then future is often the right thing to do

  // *** rpc

  // rpc with implicit completion
  auto VAR = rpc(0,[](){}); // NO WARN
  rpc(0,[](){}).wait(); // NO WARN
  (void)rpc(0,[](){}); // NO WARN
  auto VAR = rpc(world(),0,[](){}); // NO WARN
  rpc(world(),0,[](){}).wait(); // NO WARN
  (void)rpc(world(),0,[](){}); // NO WARN

  // rpc with explicit default completion
  auto VAR = rpc(0,operation_cx::as_future(),[](){}); // NO WARN
  rpc(0,operation_cx::as_future(),[](){}).wait(); // NO WARN
  (void)rpc(0,operation_cx::as_future(),[](){}); // NO WARN
  auto VAR = rpc(world(),0,operation_cx::as_future(),[](){}); // NO WARN
  rpc(world(),0,operation_cx::as_future(),[](){}).wait(); // NO WARN
  (void)rpc(world(),0,operation_cx::as_future(),[](){}); // NO WARN

  // rpc with non-default completions
  rpc(0,operation_cx::as_promise(p),[](){}); // NO WARN
  rpc(world(),0,operation_cx::as_promise(p),[](){}); // NO WARN

  rpc(0,operation_cx::as_promise(p)|operation_cx::as_promise(p2),[](){}); // NO WARN
  rpc(world(),0,operation_cx::as_promise(p)|operation_cx::as_promise(p2),[](){}); // NO WARN

  rpc(0,operation_cx::as_promise(p)|source_cx::as_promise(p2),[](){}); // NO WARN
  rpc(world(),0,operation_cx::as_promise(p)|source_cx::as_promise(p2),[](){}); // NO WARN

  auto VAR = rpc(0,operation_cx::as_future()|source_cx::as_future(),[](){}); // NO WARN
  auto VAR = rpc(world(),0,source_cx::as_future()|operation_cx::as_future(),[](){}); // NO WARN
  (void)rpc(0,operation_cx::as_future()|source_cx::as_future(),[](){}); // NO WARN
  (void)rpc(world(),0,source_cx::as_future()|operation_cx::as_future(),[](){}); // NO WARN

  // *** rpc_ff
  
  rpc_ff(0,[](){}); // NO WARN
  rpc_ff(world(),0,[](){}); // NO WARN

  rpc_ff(0,source_cx::as_promise(p),[](){}); // NO WARN
  rpc_ff(world(),0,source_cx::as_promise(p),[](){}); // NO WARN

  auto VAR = rpc_ff(0,source_cx::as_future(),[](){}); // NO WARN
  auto VAR = rpc_ff(world(),0,source_cx::as_future(),[](){}); // NO WARN
  (void)rpc_ff(0,source_cx::as_future(),[](){}); // NO WARN
  (void)rpc_ff(world(),0,source_cx::as_future(),[](){}); // NO WARN

  // *** rput

  // implicit completion
  auto VAR = rput(42,gp);  // NO WARN
  auto VAR = rput(lp,gp,sz); // NO WARN
  (void)rput(42,gp); // NO WARN
  (void)rput(lp,gp,sz); // NO WARN

  // explicit completion
  rput(42,gp,operation_cx::as_promise(p));  // NO WARN
  rput(lp,gp,sz,operation_cx::as_promise(p)); // NO WARN
  rput(42,gp,remote_cx::as_rpc([](){})); // NO WARN
  rput(lp,gp,sz,remote_cx::as_rpc([](){})); // NO WARN
  rput(42,gp,operation_cx::as_promise(p)|source_cx::as_promise(p2)); // NO WARN
  rput(lp,gp,sz,source_cx::as_promise(p2)|operation_cx::as_promise(p)); // NO WARN
  rput(42,gp,remote_cx::as_rpc([](){})|source_cx::as_promise(p2)); // NO WARN
  rput(lp,gp,sz,source_cx::as_promise(p2)|operation_cx::as_promise(p)|remote_cx::as_rpc([](){})); // NO WARN
  auto VAR = rput(42,gp,operation_cx::as_future()|source_cx::as_future()); // NO WARN
  auto VAR = rput(lp,gp,sz,source_cx::as_future()|operation_cx::as_future()); // NO WARN
  (void)rput(42,gp,operation_cx::as_future()); // NO WARN
  (void)rput(lp,gp,sz,operation_cx::as_future()); // NO WARN
  (void)rput(42,gp,operation_cx::as_future()|source_cx::as_promise(p2)); // NO WARN
  (void)rput(lp,gp,sz,source_cx::as_future()|operation_cx::as_promise(p)); // NO WARN
  (void)rput(42,gp,operation_cx::as_future()|source_cx::as_future()); // NO WARN
  (void)rput(lp,gp,sz,source_cx::as_future()|operation_cx::as_future()); // NO WARN

  // *** rget

  // implicit completion
  auto VAR = rget(gp);  // NO WARN
  auto VAR = rget(gp,lp,sz); // NO WARN
  (void)rget(gp);  // NO WARN
  (void)rget(gp,lp,sz); // NO WARN

  // explicit completion
  rget(gp,operation_cx::as_promise(p));  // NO WARN
  rget(gp,lp,sz,operation_cx::as_promise(p)); // NO WARN
  rget(gp,operation_cx::as_promise(p)|operation_cx::as_promise(p2)); // NO WARN
  rget(gp,lp,sz,operation_cx::as_promise(p2)|operation_cx::as_promise(p)); // NO WARN
  auto VAR = rget(gp,operation_cx::as_future()|operation_cx::as_future()); // NO WARN
  auto VAR = rget(gp,lp,sz,operation_cx::as_future()|operation_cx::as_future()); // NO WARN
  (void)rget(gp,operation_cx::as_future()); // NO WARN
  (void)rget(gp,lp,sz,operation_cx::as_future()); // NO WARN
  (void)rget(gp,operation_cx::as_future()|operation_cx::as_promise(p2)); // NO WARN
  (void)rget(gp,lp,sz,operation_cx::as_future()|operation_cx::as_promise(p)); // NO WARN
  (void)rget(gp,operation_cx::as_future()|operation_cx::as_future()); // NO WARN
  (void)rget(gp,lp,sz,operation_cx::as_future()|operation_cx::as_future()); // NO WARN

  // *** vis
  
  std::pair<int *,size_t> lpp(lp,sz);
  std::pair<global_ptr<int>,size_t> gpp(gp,sz);
  std::array<std::ptrdiff_t,1> a_stride = {{4}};
  std::array<std::size_t,1> a_ext = {{1}};
  std::ptrdiff_t stride[1] = {4};
  std::size_t ext[1] = {1};

  auto VAR = rput_irregular(&lpp,&lpp+1,&gpp,&gpp+1); // NO WARN
  rput_irregular(&lpp,&lpp+1,&gpp,&gpp+1,operation_cx::as_promise(p)); // NO WARN
  auto VAR = rput_regular(&lp,&lp+1,sz,&gp,&gp+1,sz); // NO WARN
  rput_regular(&lp,&lp+1,sz,&gp,&gp+1,sz,operation_cx::as_promise(p)); // NO WARN
  auto VAR = rput_strided(lp, a_stride, gp, a_stride, a_ext); // NO WARN
  rput_strided(lp, a_stride, gp, a_stride, a_ext, operation_cx::as_promise(p)); // NO WARN
  auto VAR = rput_strided<1>(lp, stride, gp, stride, ext); // NO WARN
  rput_strided<1>(lp, stride, gp, stride, ext, operation_cx::as_promise(p)); // NO WARN

  auto VAR = rget_irregular(&gpp,&gpp+1,&lpp,&lpp+1); // NO WARN
  rget_irregular(&gpp,&gpp+1,&lpp,&lpp+1,operation_cx::as_promise(p)); // NO WARN
  auto VAR = rget_regular(&gp,&gp+1,sz,&lp,&lp+1,sz); // NO WARN
  rget_regular(&gp,&gp+1,sz,&lp,&lp+1,sz,operation_cx::as_promise(p)); // NO WARN
  auto VAR = rget_strided(gp, a_stride, lp, a_stride, a_ext); // NO WARN
  rget_strided(gp, a_stride, lp, a_stride, a_ext, operation_cx::as_promise(p)); // NO WARN
  auto VAR = rget_strided<1>(gp, stride, lp, stride, ext); // NO WARN
  rget_strided<1>(gp, stride, lp, stride, ext, operation_cx::as_promise(p)); // NO WARN

  // *** dist_object

  auto VAR = dobj.fetch(0); // NO WARN

  // *** lpc

  auto VAR = master_persona().lpc([](){}); // NO WARN
  master_persona().lpc([](){}).wait(); // NO WARN
  (void)master_persona().lpc([](){}); // NO WARN

  // *** atomics

  std::memory_order order = std::memory_order_relaxed;
  auto VAR = ad.load(gp, order); // NO WARN
  ad.load(gp, order, operation_cx::as_promise(p)); // NO WARN
  auto VAR = ad.store(gp, 42, order); // NO WARN
  ad.store(gp, 42, order, operation_cx::as_promise(p)); // NO WARN
  auto VAR = ad.add(gp, 42, order); // NO WARN
  ad.add(gp, 42, order, operation_cx::as_promise(p)); // NO WARN
  auto VAR = ad.fetch_add(gp, 42, order); // NO WARN
  ad.fetch_add(gp, 42, order, operation_cx::as_promise(p)); // NO WARN
  auto VAR = ad.inc(gp, order); // NO WARN
  ad.inc(gp, order, operation_cx::as_promise(p)); // NO WARN
  auto VAR = ad.fetch_inc(gp, order); // NO WARN
  ad.fetch_inc(gp, order, operation_cx::as_promise(p)); // NO WARN
  auto VAR = ad.compare_exchange(gp, 42, 42, order); // NO WARN
  ad.compare_exchange(gp, 42, 42, order, operation_cx::as_promise(p)); // NO WARN

  // *** collectives

  auto VAR = barrier_async(); // NO WARN
  barrier_async(world(), operation_cx::as_promise(p)); // NO WARN

  auto VAR = broadcast(42, 0); // NO WARN
  auto VAR = broadcast(42, 0, world()); // NO WARN
  auto VAR = broadcast(lp,sz, 0); // NO WARN
  auto VAR = broadcast(lp,sz, 0, world()); // NO WARN

  broadcast(42, 0, world(), operation_cx::as_promise(p)); // NO WARN
  broadcast(lp,sz, 0, world(), operation_cx::as_promise(p)); // NO WARN
  auto VAR = broadcast(42, 0, world(), operation_cx::as_promise(p)|operation_cx::as_future()); // NO WARN
  auto VAR = broadcast(lp,sz, 0, world(), operation_cx::as_promise(p)|operation_cx::as_future()); // NO WARN

  auto VAR = reduce_one(42, op_fast_add, 0); // NO WARN
  auto VAR = reduce_all(42, op_fast_add); // NO WARN
  auto VAR = reduce_one(42, op_fast_add, 0, world()); // NO WARN
  auto VAR = reduce_all(42, op_fast_add, world()); // NO WARN
  auto VAR = reduce_one(lp,lp,sz, op_fast_add, 0); // NO WARN
  auto VAR = reduce_all(lp,lp,sz, op_fast_add); // NO WARN
  auto VAR = reduce_one(lp,lp,sz, op_fast_add, 0, world()); // NO WARN
  auto VAR = reduce_all(lp,lp,sz, op_fast_add, world()); // NO WARN

  reduce_one(42, op_fast_add, 0, world(), operation_cx::as_promise(p)); // NO WARN
  reduce_all(42, op_fast_add, world(), operation_cx::as_promise(p)); // NO WARN
  reduce_one(lp,lp,sz, op_fast_add, 0, world(), operation_cx::as_promise(p)); // NO WARN
  reduce_all(lp,lp,sz, op_fast_add, world(), operation_cx::as_promise(p)); // NO WARN
  auto VAR = reduce_one(42, op_fast_add, 0, world(), operation_cx::as_promise(p)|operation_cx::as_future()); // NO WARN
  auto VAR = reduce_all(42, op_fast_add, world(), operation_cx::as_promise(p)|operation_cx::as_future()); // NO WARN
  auto VAR = reduce_one(lp,lp,sz, op_fast_add, 0, world(), operation_cx::as_promise(p)|operation_cx::as_future()); // NO WARN
  auto VAR = reduce_all(lp,lp,sz, op_fast_add, world(), operation_cx::as_promise(p)|operation_cx::as_future()); // NO WARN

  // *** allocation

  auto VAR = new_<int>(1); // NO WARN
  auto VAR = new_<int>(std::nothrow,1); // NO WARN
  auto VAR = new_array<int>(1); // NO WARN
  auto VAR = new_array<int>(1,std::nothrow); // NO WARN
  auto VAR = allocate(1); // NO WARN
  auto VAR = allocate<int>(1); // NO WARN

  // *** memory kinds

  device_allocator<cuda_device> da;

  auto VAR = da.allocate<int>(1); // NO WARN

  auto VAR = copy(gp, gp, 1); // NO WARN
  auto VAR = copy(gp, gp, 1, operation_cx::as_future()); // NO WARN
  copy(gp, gp, 1, operation_cx::as_promise(p)); // NO WARN
  auto VAR = copy(lp, gp, 1); // NO WARN
  auto VAR = copy(lp, gp, 1, operation_cx::as_future()); // NO WARN
  copy(lp, gp, 1, operation_cx::as_promise(p)); // NO WARN
  auto VAR = copy(gp, lp, 1); // NO WARN
  auto VAR = copy(gp, lp, 1, operation_cx::as_future()); // NO WARN
  copy(gp, lp, 1, operation_cx::as_promise(p)); // NO WARN

  // ---------------------------------------------------------------------------------
  #if WARN
  // All of the following cases drop at least one future return from comm initiation.
  // There are two cases:
  // 1. Dropping the only operation_cx event, which is a future (this is the default completion)
  //     This is always an error, because it leads to a persona that cannot be 
  //     quiesced - after such an action, calling upcxx::finalize() or otherwise destroying 
  //     the persona leads to undefined behavior (and likely crashes in many cases).
  // 2. Explicitly requesting a "duplicate" future from comm initiation, along with 
  //    another (non-future) operation_cx completion, and then dropping the future(s).
  //     This case is not guaranteed to lead to undefined behavior. 
  //     However it is wasteful and almost certainly represents a programming mistake.
  //     The warning can be easily silenced by either removing the request for the dropped
  //     future (preferred), or casting the return to `(void)` to make the drop explicit.

  // *** rpc

  rpc(0,[](){}); // WARN
  rpc(world(),0,[](){}); // WARN
  rpc(0,operation_cx::as_future(),[](){}); // WARN
  rpc(world(),0,operation_cx::as_future(),[](){}); // WARN

  rpc(0,operation_cx::as_promise(p)|operation_cx::as_future(),[](){}); // WARN
  rpc(world(),0,operation_cx::as_future()|operation_cx::as_promise(p),[](){}); // WARN

  rpc(0,operation_cx::as_promise(p)|source_cx::as_future(),[](){}); // WARN
  rpc(world(),0,source_cx::as_future()|operation_cx::as_promise(p),[](){}); // WARN

  rpc(0,operation_cx::as_future()|source_cx::as_future(),[](){}); // WARN
  rpc(world(),0,source_cx::as_future()|operation_cx::as_future(),[](){}); // WARN

  // *** rpc_ff

  rpc_ff(0,source_cx::as_future(),[](){}); // WARN
  rpc_ff(world(),0,source_cx::as_future(),[](){}); // WARN

  // *** rput

  rput(42,gp); // WARN
  rput(lp,gp,sz); // WARN

  rput(42,gp,operation_cx::as_future()); // WARN
  rput(lp,gp,sz,operation_cx::as_future()); // WARN
  rput(42,gp,operation_cx::as_future()|source_cx::as_promise(p2)); // WARN
  rput(lp,gp,sz,source_cx::as_future()|operation_cx::as_promise(p)); // WARN
  rput(42,gp,operation_cx::as_future()|source_cx::as_future()); // WARN
  rput(lp,gp,sz,source_cx::as_future()|operation_cx::as_future()); // WARN

  // *** rget

  rget(gp);  // WARN
  rget(gp,lp,sz); // WARN

  rget(gp,operation_cx::as_future()); // WARN
  rget(gp,lp,sz,operation_cx::as_future()); // WARN
  rget(gp,operation_cx::as_future()|operation_cx::as_promise(p2)); // WARN
  rget(gp,lp,sz,operation_cx::as_future()|operation_cx::as_promise(p)); // WARN
  rget(gp,operation_cx::as_future()|operation_cx::as_future()); // WARN
  rget(gp,lp,sz,operation_cx::as_future()|operation_cx::as_future()); // WARN

  // *** vis
  
  rput_irregular(&lpp,&lpp+1,&gpp,&gpp+1); // WARN
  rput_irregular(&lpp,&lpp+1,&gpp,&gpp+1,operation_cx::as_future()); // WARN
  rput_regular(&lp,&lp+1,sz,&gp,&gp+1,sz); // WARN
  rput_regular(&lp,&lp+1,sz,&gp,&gp+1,sz,operation_cx::as_future()); // WARN
  rput_strided(lp, a_stride, gp, a_stride, a_ext); // WARN
  rput_strided(lp, a_stride, gp, a_stride, a_ext, operation_cx::as_future()); // WARN
  rput_strided<1>(lp, stride, gp, stride, ext); // WARN
  rput_strided<1>(lp, stride, gp, stride, ext, operation_cx::as_future()); // WARN

  rget_irregular(&gpp,&gpp+1,&lpp,&lpp+1); // WARN
  rget_irregular(&gpp,&gpp+1,&lpp,&lpp+1,operation_cx::as_future()); // WARN
  rget_regular(&gp,&gp+1,sz,&lp,&lp+1,sz); // WARN
  rget_regular(&gp,&gp+1,sz,&lp,&lp+1,sz,operation_cx::as_future()); // WARN
  rget_strided(gp, a_stride, lp, a_stride, a_ext); // WARN
  rget_strided(gp, a_stride, lp, a_stride, a_ext, operation_cx::as_future()); // WARN
  rget_strided<1>(gp, stride, lp, stride, ext); // WARN
  rget_strided<1>(gp, stride, lp, stride, ext, operation_cx::as_future()); // WARN

  // *** dist_object

  dobj.fetch(0); // WARN

  // *** lpc

  master_persona().lpc([](){}); // WARN

  // *** atomics

  ad.load(gp, order); // WARN
  ad.load(gp, order, operation_cx::as_future()); // WARN
  ad.store(gp, 42, order); // WARN
  ad.store(gp, 42, order, operation_cx::as_future()); // WARN
  ad.add(gp, 42, order); // WARN
  ad.add(gp, 42, order, operation_cx::as_future()); // WARN
  ad.fetch_add(gp, 42, order); // WARN
  ad.fetch_add(gp, 42, order, operation_cx::as_future()); // WARN
  ad.inc(gp, order); // WARN
  ad.inc(gp, order, operation_cx::as_future()); // WARN
  ad.fetch_inc(gp, order); // WARN
  ad.fetch_inc(gp, order, operation_cx::as_future()); // WARN
  ad.compare_exchange(gp, 42, 42, order); // WARN
  ad.compare_exchange(gp, 42, 42, order, operation_cx::as_future()); // WARN

  // *** collectives

  barrier_async(); // WARN
  barrier_async(world(),operation_cx::as_future()); // WARN
  barrier_async(world(),operation_cx::as_promise(p)|operation_cx::as_future()); // WARN

  broadcast(42, 0); // WARN
  broadcast(42, 0, world()); // WARN
  broadcast(lp,sz, 0); // WARN
  broadcast(lp,sz, 0, world()); // WARN

  broadcast(42, 0, world(), operation_cx::as_promise(p)|operation_cx::as_future()); // WARN
  broadcast(lp,sz, 0, world(), operation_cx::as_promise(p)|operation_cx::as_future()); // WARN

  reduce_one(42, op_fast_add, 0); // WARN
  reduce_all(42, op_fast_add); // WARN
  reduce_one(42, op_fast_add, 0, world()); // WARN
  reduce_all(42, op_fast_add, world()); // WARN
  reduce_one(lp,lp,sz, op_fast_add, 0); // WARN
  reduce_all(lp,lp,sz, op_fast_add); // WARN
  reduce_one(lp,lp,sz, op_fast_add, 0, world()); // WARN
  reduce_all(lp,lp,sz, op_fast_add, world()); // WARN

  reduce_one(42, op_fast_add, 0, world(), operation_cx::as_promise(p)|operation_cx::as_future()); // WARN
  reduce_all(42, op_fast_add, world(), operation_cx::as_promise(p)|operation_cx::as_future()); // WARN
  reduce_one(lp,lp,sz, op_fast_add, 0, world(), operation_cx::as_promise(p)|operation_cx::as_future()); // WARN
  reduce_all(lp,lp,sz, op_fast_add, world(), operation_cx::as_promise(p)|operation_cx::as_future()); // WARN

  // *** allocation

  new_<int>(1); // WARN
  new_<int>(std::nothrow,1); // WARN
  new_array<int>(1); // WARN
  new_array<int>(1,std::nothrow); // WARN
  allocate(1); // WARN
  allocate<int>(1); // WARN

  // *** memory kinds

  da.allocate<int>(1); // WARN

  copy(gp, gp, 1); // WARN
  copy(gp, gp, 1, operation_cx::as_future()); // WARN
  copy(lp, gp, 1); // WARN
  copy(lp, gp, 1, operation_cx::as_future()); // WARN
  copy(gp, lp, 1); // WARN
  copy(gp, lp, 1, operation_cx::as_future()); // WARN

  #endif // #if WARN
  // ---------------------------------------------------------------------------------
  }
  
  upcxx::finalize();
  return 0;
}
