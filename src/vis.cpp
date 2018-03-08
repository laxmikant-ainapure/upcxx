#include <upcxx/vis.hpp>
#include <upcxx/backend/gasnet/runtime_internal.hpp>
#if UPCXX_BACKEND_GASNET 
#include <gasnet_vis.h> //
#endif

namespace gasnet = upcxx::backend::gasnet;

static_assert(offsetof(gex_Memvec_t, gex_addr) == offsetof(upcxx::detail::memvec_t, gex_addr) &&
              offsetof(gex_Memvec_t, gex_len) == offsetof(upcxx::detail::memvec_t, gex_len) &&
              sizeof(gex_Memvec_t) == sizeof(upcxx::detail::memvec_t),
              "UPC++ internal issue: unsupported gasnet version");

void upcxx::detail::rma_put_irreg_nb(
                                    upcxx::intrank_t rank_d,
                                    std::size_t _dstcount,
                                    upcxx::detail::memvec_t const _dstlist[],
                                    std::size_t _srccount,
                                    upcxx::detail::memvec_t const _srclist[],
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


  if(source_cb!=NULL) // user has asked for source completion
    {
      source_cb->handle = reinterpret_cast<uintptr_t>(op_h);
      gasnet::register_cb(source_cb);

      //  we rely on upcxx source completion pushing the operation completion into the queue, which will
      //  be automatically triggered since EVENT INVALID is always ready..I think.  bvs
      operation_cb->handle = reinterpret_cast<uintptr_t>(GEX_EVENT_INVALID);
    } else {
    operation_cb->handle = reinterpret_cast<uintptr_t>(op_h);
    gasnet::register_cb(operation_cb);
  }
  gasnet::after_gasnet();
}



void upcxx::detail::rma_get_irreg_nb(                               
                                    std::size_t _dstcount,
                                    upcxx::detail::memvec_t const _dstlist[],
                                    upcxx::intrank_t rank_s,
                                    std::size_t _srccount,
                                    upcxx::detail::memvec_t const _srclist[],
                                    backend::gasnet::handle_cb *operation_cb)
{

  gex_Event_t op_h = gex_VIS_VectorGetNB(gasnet::world_team,
                                         _dstcount,
                                         reinterpret_cast<const gex_Memvec_t*>(_dstlist),
                                         rank_s,
                                         _srccount,
                                         reinterpret_cast<const gex_Memvec_t*>(_srclist),
                                         /* flags */ 0);

  operation_cb->handle = reinterpret_cast<uintptr_t>(op_h);
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
  if(source_cb!=NULL)
    {
      source_cb->handle = reinterpret_cast<uintptr_t>(op_h);
      gasnet::register_cb(source_cb);
      operation_cb->handle =  reinterpret_cast<uintptr_t>(GEX_EVENT_INVALID);
    } else {
    operation_cb->handle = reinterpret_cast<uintptr_t>(op_h);
    gasnet::register_cb(operation_cb);
  }
  gasnet::after_gasnet();

}

void upcxx::detail::rma_get_reg_nb(
                    size_t _dstcount, void * const _dstlist[], size_t _dstlen,
                    intrank_t rank_s,
                    size_t _srccount, void * const _srclist[], size_t _srclen,
                    backend::gasnet::handle_cb *operation_cb)
{
  gex_Event_t op_h = gex_VIS_IndexedGetNB(gasnet::world_team,
                                         _dstcount, _dstlist, _dstlen,
                                          rank_s,
                                         _srccount, _srclist, _srclen,
                                         /* flags*/ 0);

  operation_cb->handle = reinterpret_cast<uintptr_t>(op_h);
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
   if(source_cb!=NULL)
    {
      source_cb->handle = reinterpret_cast<uintptr_t>(op_h);
      gasnet::register_cb(source_cb);
      operation_cb->handle =  reinterpret_cast<uintptr_t>(GEX_EVENT_INVALID);
    } else {

     operation_cb->handle = reinterpret_cast<uintptr_t>(op_h);
     gasnet::register_cb(operation_cb);
   }
  gasnet::after_gasnet();

}

void upcxx::detail::rma_get_strided_nb(
                        void *_dstaddr, const std::ptrdiff_t _dststrides[],
                        intrank_t _rank_s,
                        const void *_srcaddr, const std::ptrdiff_t _srcstrides[],
                        std::size_t _elemsz,
                        const std::size_t _count[], std::size_t _stridelevels,
                        backend::gasnet::handle_cb *operation_cb)
{
  gex_Event_t op_h = gex_VIS_StridedGetNB(gasnet::world_team,
                                          _dstaddr, _dststrides,
                                          _rank_s,
                                          const_cast<void*>(_srcaddr), _srcstrides,
                                          _elemsz,
                                          _count, _stridelevels,
                                          /*flag */ 0);
 

  operation_cb->handle = reinterpret_cast<uintptr_t>(op_h);
  gasnet::register_cb(operation_cb);
  gasnet::after_gasnet();

}

