#include <upcxx/vis.hpp>
#include <upcxx/backend/gasnet/runtime_internal.hpp>
#include <gasnet_vis.h>
namespace gasnet = upcxx::backend::gasnet;

void upcxx::detail::rma_put_frag_nb(
                                    upcxx::intrank_t rank_d,
                                    std::size_t _dstcount,
                                    upcxx::backend::memvec_t const _dstlist[],
                                    std::size_t _srccount,
                                    upcxx::backend::memvec_t const _srclist[],
                                    backend::gasnet::handle_cb *source_cb,
                                    backend::gasnet::handle_cb *operation_cb)
{

  // this call will eventually get a source completion signature.
  gex_Event_t op_h = gex_VIS_VectorPutNB(gasnet::world_team,
                                         rank_d,
                                         _dstcount,
                                         reinterpret_cast<const gex_Memvec_t*>(_dstlist),
                                         _srccount,
                                         reinterpret_cast<const gex_Memvec_t*>(_srclist),
                                         /* flags */ 0);

  source_cb->handle = reinterpret_cast<uintptr_t>(op_h);
  operation_cb->handle = reinterpret_cast<uintptr_t>(op_h);

  gasnet::register_cb(source_cb);
  gasnet::register_cb(operation_cb);

  gasnet::after_gasnet();
}

void upcxx::detail::rma_put_reg_nb(
                    intrank_t rank_d,
                    size_t _dstcount, void * const _dstlist[], size_t _dstlen,
                    size_t _srccount, void * const _srclist[], size_t _srclen,
                    backend::gasnet::handle_cb *source_cb,
                    backend::gasnet::handle_cb *operation_cb)
{
  gex_Event_t op_h = gex_VIS_IndexedPutNB(gasnet::world_team,
                                         rank_d,
                                         _dstcount, _dstlist, _dstlen,
                                         _srccount, _srclist, _srclen,
                                         /* flags*/ 0);

  source_cb->handle = reinterpret_cast<uintptr_t>(op_h);
  operation_cb->handle = reinterpret_cast<uintptr_t>(op_h);

  gasnet::register_cb(source_cb);
  gasnet::register_cb(operation_cb);

  gasnet::after_gasnet();

}

void upcxx::detail::rma_put_strided_nb(
                        intrank_t rank_d,
                        void *_dstaddr, const std::ptrdiff_t _dststrides[],
                        const void *_srcaddr, const std::ptrdiff_t _srcstrides[],
                        std::size_t _elemsz,
                        const std::size_t _count[], std::size_t _stridelevels,
                        backend::gasnet::handle_cb *source_cb,
                        backend::gasnet::handle_cb *operation_cb)
{
  gex_Event_t op_h = gex_VIS_StridedPutNB(gasnet::world_team,
                                          rank_d,
                                          _dstaddr, _dststrides,
                                          const_cast<void*>(_srcaddr), _srcstrides,
                                          _elemsz,
                                          _count, _stridelevels,
                                          /*flag */ 0);
  
  source_cb->handle = reinterpret_cast<uintptr_t>(op_h);
  operation_cb->handle = reinterpret_cast<uintptr_t>(op_h);

  gasnet::register_cb(source_cb);
  gasnet::register_cb(operation_cb);

  gasnet::after_gasnet();

}
