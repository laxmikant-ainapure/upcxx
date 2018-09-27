#ifndef _CECA99E6_CB27_41E2_9478_E2A9B106BBF2
#define _CECA99E6_CB27_41E2_9478_E2A9B106BBF2

#include <upcxx/completion.hpp>
#include <upcxx/global_ptr.hpp>
#include <upcxx/rput.hpp>
#include <upcxx/rget.hpp>
#include <tuple>
#include <type_traits>
#include <vector>

//  I'm not sure we will have a Vector-Index-Strided interface that is
//  not GASNet-based, so if an application wants to make use of the VIS
//  interface it is likely they will want to be running GASNet-Ex 
#include <upcxx/backend/gasnet/runtime.hpp>

namespace upcxx
{
  
  namespace detail {

    struct memvec_t {
      memvec_t() { }
      const void  *gex_addr;  // TODO: When gasnet changes Memvec we need to track it
      size_t gex_len;
    };

    
    void rma_put_irreg_nb(
                         intrank_t rank_d,
                         std::size_t _dstcount,
                         upcxx::detail::memvec_t const _dstlist[],
                         std::size_t _srcount,
                         upcxx::detail::memvec_t const _srclist[],
                         backend::gasnet::handle_cb *source_cb,
                         backend::gasnet::handle_cb *operation_cb);
    void rma_get_irreg_nb(                               
                         std::size_t _dstcount,
                         upcxx::detail::memvec_t const _dstlist[],
                         upcxx::intrank_t rank_s,
                         std::size_t _srccount,
                         upcxx::detail::memvec_t const _srclist[],
                         backend::gasnet::handle_cb *operation_cb);
    
    
    void rma_put_reg_nb(
                        intrank_t rank_d,
                        size_t _dstcount, void * const _dstlist[], size_t _dstlen,
                        size_t _srccount, void * const _srclist[], size_t _srclen,
                        backend::gasnet::handle_cb *source_cb,
                        backend::gasnet::handle_cb *operation_cb);
    
    void rma_get_reg_nb(
                        size_t _dstcount, void * const _dstlist[], size_t _dstlen,
                        intrank_t ranks,
                        size_t _srccount, void * const _srclist[], size_t _srclen,
                        backend::gasnet::handle_cb *operation_cb);
    
    void rma_put_strided_nb(
                            intrank_t rank_d,
                            void *_dstaddr, const std::ptrdiff_t _dststrides[],
                            const void *_srcaddr, const std::ptrdiff_t _srcstrides[],
                            std::size_t _elemsz,
                            const std::size_t _count[], std::size_t _stridelevels,
                            backend::gasnet::handle_cb *source_cb,
                            backend::gasnet::handle_cb *operation_cb);
    void rma_get_strided_nb(
                            void *_dstaddr, const std::ptrdiff_t _dststrides[],
                            intrank_t _rank_s,
                            const void *_srcaddr, const std::ptrdiff_t _srcstrides[],
                            std::size_t _elemsz,
                            const std::size_t _count[], std::size_t _stridelevels,
                            backend::gasnet::handle_cb *operation_cb);
    

    template<typename CxStateHere, typename CxStateRemote>
    struct rput_cbs_irreg final:
      rput_cb_source</*FinalType=*/rput_cbs_irreg<CxStateHere, CxStateRemote>,
                                   CxStateHere, CxStateRemote>,
      rput_cb_operation</*FinalType=*/rput_cbs_irreg<CxStateHere, CxStateRemote>,
                                      CxStateHere, CxStateRemote>
    {
      intrank_t rank_d;
      CxStateHere state_here;
      CxStateRemote state_remote;
      std::vector<upcxx::detail::memvec_t> src;
      std::vector<upcxx::detail::memvec_t> dest;
      rput_cbs_irreg(intrank_t rank_d, CxStateHere here, CxStateRemote remote,
                     std::vector<upcxx::detail::memvec_t>&& src,
                     std::vector<upcxx::detail::memvec_t>&& dest):
        rank_d(rank_d),
        state_here(std::move(here)),
        state_remote(std::move(remote)),
        src(src),
        dest(dest) {
      }
      static constexpr bool static_scope = false;
      void initiate()
      {
        detail::rma_put_irreg_nb(rank_d, dest.size(), &(dest[0]),
                                src.size(), &(src[0]),
                                this->source_cb(), this->operation_cb()); 
      }

    };

    template<typename CxStateHere, typename CxStateRemote>
    struct rput_cbs_reg final:
      rput_cb_source</*FinalType=*/rput_cbs_reg<CxStateHere, CxStateRemote>,
                                   CxStateHere, CxStateRemote>,
      rput_cb_operation</*FinalType=*/rput_cbs_reg<CxStateHere, CxStateRemote>,
                                      CxStateHere, CxStateRemote>
    {
      intrank_t rank_d;
      CxStateHere state_here;
      CxStateRemote state_remote;
      std::vector<void*> src;
      std::vector<void*> dest;
      rput_cbs_reg(intrank_t rank_d, CxStateHere here, CxStateRemote remote,
                    std::vector<void*>&& src,
                    std::vector<void*>&& dest):
        rank_d(rank_d),
        state_here(std::move(here)),
        state_remote(std::move(remote)),
        src(src),
        dest(dest) {
      }
      static constexpr bool static_scope = false;
      void initiate(std::size_t destlen, std::size_t srclen)
      {
        detail::rma_put_reg_nb(rank_d, dest.size(), &(dest[0]), destlen,
                               src.size(), &(src[0]), srclen,
                               this->source_cb(), this->operation_cb());
      }

    };
    
    template<typename CxStateHere, typename CxStateRemote>
    struct rput_cbs_strided final:
      rput_cb_source</*FinalType=*/rput_cbs_strided<CxStateHere, CxStateRemote>,
                                   CxStateHere, CxStateRemote>,
      rput_cb_operation</*FinalType=*/rput_cbs_strided<CxStateHere, CxStateRemote>,
                                      CxStateHere, CxStateRemote>
    {
      intrank_t rank_d;
      CxStateHere state_here;
      CxStateRemote state_remote;
      rput_cbs_strided(intrank_t rank_d, CxStateHere here, CxStateRemote remote):
        rank_d(rank_d),
        state_here(std::move(here)),
        state_remote(std::move(remote))
      {
      }
      static constexpr bool static_scope = false;
      void initiate(intrank_t rd,
                    void* dst_addr, const std::ptrdiff_t* dststrides,
                    const void* src_addr, const std::ptrdiff_t* srcstrides,
                    std::size_t elemsize,
                    const std::size_t* count, std::size_t stridelevels)
      {
        detail::rma_put_strided_nb(rd, dst_addr, dststrides,
                                   src_addr, srcstrides,
                                   elemsize, count, stridelevels,
                                   this->source_cb(), this->operation_cb()); 
      }

    };
    
    template<typename CxStateHere, typename CxStateRemote>
    struct rget_cb_irreg final: rget_cb_remote<CxStateRemote>, backend::gasnet::handle_cb {
      CxStateHere state_here;
      std::vector<upcxx::detail::memvec_t> src;
      std::vector<upcxx::detail::memvec_t> dest;
      rget_cb_irreg(intrank_t rank_s, CxStateHere here, CxStateRemote remote,
                   std::vector<upcxx::detail::memvec_t>&& Src,
                   std::vector<upcxx::detail::memvec_t>&& Dest)
        : rget_cb_remote<CxStateRemote>{rank_s, std::move(remote)},
        state_here{std::move(here)}, src(Src), dest(Dest) { }
      void initiate(intrank_t rank_s)
      {
        detail::rma_get_irreg_nb(dest.size(), &(dest[0]),
                                rank_s  ,src.size(), &(src[0]),
                                this);
      }
      void execute_and_delete(backend::gasnet::handle_cb_successor) {
        this->send_remote();
        this->state_here.template operator()<operation_cx_event>();
        delete this;
      }
    };

    template<typename CxStateHere, typename CxStateRemote>
    struct rget_cb_reg final: rget_cb_remote<CxStateRemote>, backend::gasnet::handle_cb {
      CxStateHere state_here;
      std::vector<void*> src;
      std::vector<void*> dest;
      rget_cb_reg(intrank_t rank_s, CxStateHere here, CxStateRemote remote,
                   std::vector<void*>&& Src,
                   std::vector<void*>&& Dest)
        : rget_cb_remote<CxStateRemote>{rank_s, std::move(remote)},
        state_here{std::move(here)}, src(Src), dest(Dest) { }
      void initiate(intrank_t rank_s, std::size_t srclength, std::size_t dstlength)
      {
        detail::rma_get_reg_nb(dest.size(), &(dest[0]), dstlength, 
                               rank_s  ,src.size(), &(src[0]), srclength,
                               this);
      }
      void execute_and_delete(backend::gasnet::handle_cb_successor) {
        this->send_remote();
        this->state_here.template operator()<operation_cx_event>();
        delete this;
      }
    };

    template<typename CxStateHere, typename CxStateRemote>
    struct rget_cbs_strided final: rget_cb_remote<CxStateRemote>, backend::gasnet::handle_cb {
      CxStateHere state_here;
      
      rget_cbs_strided(intrank_t rank_s, CxStateHere here, CxStateRemote remote)
        : rget_cb_remote<CxStateRemote>{rank_s, std::move(remote)},
        state_here{std::move(here)} { }
      void initiate(
                    void* dst_addr, const std::ptrdiff_t* dststrides,
                    intrank_t rank_s,
                    const void* src_addr, const std::ptrdiff_t* srcstrides,
                    std::size_t elemsize,
                    const std::size_t* count, std::size_t stridelevels)
      {
        detail::rma_get_strided_nb(dst_addr, dststrides,
                                   rank_s, src_addr, srcstrides,
                                   elemsize, count, stridelevels,
                                   this); 
      }

      void execute_and_delete(backend::gasnet::handle_cb_successor) {
        this->send_remote();
        this->state_here.template operator()<operation_cx_event>();
        delete this;
      }
    };
  }
  /////////////////////////////////////////////////////////////////////
  //  Actual public API for rput_fragmented, rput_regular, rput_strided
  //
  
  template<typename SrcIter, typename DestIter,
           typename Cxs=decltype(operation_cx::as_future())>
  typename detail::completions_returner<
    /*EventPredicate=*/detail::event_is_here,
    /*EventValues=*/detail::rput_event_values,
    Cxs>::return_t  
  rput_irregular(
                  SrcIter src_runs_begin, SrcIter src_runs_end,
                  DestIter dst_runs_begin, DestIter dst_runs_end,
                  Cxs cxs=completions<future_cx<operation_cx_event>>{{}})
  {


    using T = typename std::tuple_element<0,typename std::iterator_traits<DestIter>::value_type>::type::element_type;
    using S = typename std::tuple_element<0,typename std::iterator_traits<SrcIter>::value_type>::type;
    static_assert(is_definitely_trivially_serializable<T>::value,
      "RMA operations only work on DefinitelyTriviallySerializable types.");
    
    static_assert(std::is_convertible<S, const T*>::value,
                  "SrcIter and DestIter need to be over same base T type");

    UPCXX_ASSERT((detail::completions_has_event<Cxs, operation_cx_event>::value |
                  detail::completions_has_event<Cxs, remote_cx_event>::value),
                 "Not requesting either operation or remote completion is surely an "
                 "error. You'll have know way of ever knowing when the target memory is "
                 "safe to read or write again.");

                 
    using cxs_here_t = detail::completions_state<
      /*EventPredicate=*/detail::event_is_here,
      /*EventValues=*/detail::rput_event_values,
      Cxs>;
    using cxs_remote_t = detail::completions_state<
      /*EventPredicate=*/detail::event_is_remote,
      /*EventValues=*/detail::rput_event_values,
      Cxs>;

 
    constexpr std::size_t tsize=sizeof(T);
 
 
    std::vector<upcxx::detail::memvec_t>  dest(std::distance(dst_runs_begin, dst_runs_end));
    auto dv=dest.begin();
    std::size_t dstsize=0;
    intrank_t gpdrank = 0; // zero is a valid rank for gasnet to set event flag.
    if(dest.size()!=0) gpdrank = std::get<0>(*dst_runs_begin).rank_; //hoist gpdrank assign out of loop
    for(DestIter d=dst_runs_begin; !(d==dst_runs_end); ++d,++dv)
      {
        UPCXX_ASSERT(gpdrank==std::get<0>(*d).rank_);
        dv->gex_addr=(std::get<0>(*d)).raw_ptr_;
        dv->gex_len =std::get<1>(*d)*tsize;
        dstsize+=dv->gex_len;
      }

    UPCXX_ASSERT( !(dest.size() == 0 && detail::completions_has_event<Cxs, remote_cx_event>::value),
                  "Cannot request remote completion without providing at least one global_ptr "
                  "in the destination sequence." );
    
    std::size_t srcsize=0;
    std::vector<upcxx::detail::memvec_t> src(std::distance(src_runs_begin, src_runs_end));
    auto sv=src.begin();
    for(SrcIter s=src_runs_begin; !(s==src_runs_end); ++s,++sv)
      {
        sv->gex_addr=std::get<0>(*s);
        sv->gex_len =std::get<1>(*s)*tsize;
        srcsize+=sv->gex_len;
      }
    
    UPCXX_ASSERT(dstsize==srcsize);
    
    detail::rput_cbs_irreg<cxs_here_t, cxs_remote_t> cbs_static{
      gpdrank,
        cxs_here_t(std::move(cxs)),
        cxs_remote_t(std::move(cxs)),
        std::move(src), std::move(dest)
        };

    auto *cbs = decltype(cbs_static)::static_scope
      ? &cbs_static
      : new decltype(cbs_static){std::move(cbs_static)};
    
    auto returner = detail::completions_returner<
      /*EventPredicate=*/detail::event_is_here,
      /*EventValues=*/detail::rput_event_values,
      Cxs
      >{cbs->state_here};
    
    cbs->initiate();
    
    return returner();
  }

  template<typename SrcIter, typename DestIter,
  typename Cxs=decltype(operation_cx::as_future())>
    typename detail::completions_returner<
      /*EventPredicate=*/detail::event_is_here,
      /*EventValues=*/detail::rget_byref_event_values,
      Cxs
    >::return_t
    rget_irregular(
                   SrcIter src_runs_begin, SrcIter src_runs_end,
                   DestIter dst_runs_begin, DestIter dst_runs_end,
                   Cxs cxs=completions<future_cx<operation_cx_event>>{{}})
  {

    using T = typename std::tuple_element<0,typename std::iterator_traits<SrcIter>::value_type>::type::element_type;
    using D = typename std::tuple_element<0,typename std::iterator_traits<DestIter>::value_type>::type;

    static_assert(is_definitely_trivially_serializable<T>::value,
      "RMA operations only work on DefinitelyTriviallySerializable types.");
    
    static_assert(std::is_convertible<D, const T*>::value,
                  "SrcIter and DestIter need to be over same base T type");
 
    
    UPCXX_ASSERT((detail::completions_has_event<Cxs, operation_cx_event>::value |
                  detail::completions_has_event<Cxs, remote_cx_event>::value),
                 "Not requesting either operation or remote completion is surely an "
                 "error. You'll have know way of ever knowing when the target memory is "
                 "safe to read or write again.");

    
    using cxs_here_t = detail::completions_state<
      /*EventPredicate=*/detail::event_is_here,
      /*EventValues=*/detail::rget_byref_event_values,
      Cxs>;
    using cxs_remote_t = detail::completions_state<
      /*EventPredicate=*/detail::event_is_remote,
      /*EventValues=*/detail::rget_byref_event_values,
      Cxs>;


    constexpr std::size_t tsize=sizeof(T);
    
    std::vector<upcxx::detail::memvec_t> dest(std::distance(dst_runs_begin, dst_runs_end));
    auto dv=dest.begin();
    std::size_t dstsize=0;
    for(DestIter d=dst_runs_begin; !(d==dst_runs_end); ++d,++dv)
      {
        dv->gex_addr=(std::get<0>(*d));
        dv->gex_len =std::get<1>(*d)*tsize;
        dstsize+=dv->gex_len;
      }
    
    std::vector<upcxx::detail::memvec_t> src(std::distance(src_runs_begin, src_runs_end));
    auto sv=src.begin();
    std::size_t srcsize=0;
    intrank_t rank_s = 0; // zero is a valid rank for gasnet to perform event completion
    if(src.size()!=0) rank_s = std::get<0>(*src_runs_begin).rank_; // hoist rank_s assign out of loop
    for(SrcIter s=src_runs_begin; !(s==src_runs_end); ++s,++sv)
      {
        UPCXX_ASSERT(rank_s==std::get<0>(*s).rank_,
                     "All ranks in rput need to target the same rank");
        sv->gex_addr=std::get<0>(*s).raw_ptr_;
        sv->gex_len =std::get<1>(*s)*tsize;
        srcsize+=sv->gex_len;
      }
    UPCXX_ASSERT(!(src.size() ==0  && detail::completions_has_event<Cxs, remote_cx_event>::value),
                 "Cannot request remote completion without providing at least one global_ptr "
                 "in the source sequence.");
    

    UPCXX_ASSERT(dstsize==srcsize);
    auto *cb = new detail::rget_cb_irreg<cxs_here_t,cxs_remote_t>{
      rank_s,
      cxs_here_t{std::move(cxs)},
      cxs_remote_t{std::move(cxs)},
      std::move(src), std::move(dest)
    };

    auto returner = detail::completions_returner<
        /*EventPredicate=*/detail::event_is_here,
        /*EventValues=*/detail::rget_byref_event_values,
        Cxs
      >{cb->state_here};

    cb->initiate(rank_s);

    
    return returner();
  }
  
  template<typename SrcIter, typename DestIter,
           typename Cxs=decltype(operation_cx::as_future())>
  typename detail::completions_returner<
    /*EventPredicate=*/detail::event_is_here,
    /*EventValues=*/detail::rput_event_values,
    Cxs>::return_t  
  rput_regular(
               SrcIter src_runs_begin, SrcIter src_runs_end,
               std::size_t src_run_length,
               DestIter dst_runs_begin, DestIter dst_runs_end,
               std::size_t dst_run_length,
               Cxs cxs=completions<future_cx<operation_cx_event>>{{}})
  {
   // This computes T by pulling it out of global_ptr<T>.
    using T = typename std::iterator_traits<DestIter>::value_type::element_type;
    
    static_assert(
                  is_definitely_trivially_serializable<T>::value,
                  "RMA operations only work on DefinitelyTriviallySerializable types."
                  );
    
    UPCXX_ASSERT((
                  detail::completions_has_event<Cxs, operation_cx_event>::value |
                  detail::completions_has_event<Cxs, remote_cx_event>::value),
                 "Not requesting either operation or remote completion is surely an "
                 "error. You'll have know way of ever knowing when the target memory is "
                 "safe to read or write again.");
 
    static_assert(std::is_convertible<
                  /*from*/typename std::iterator_traits<SrcIter>::value_type,
                  /*to*/T const*
                  >::value,
                  "Source iterator's value type not convertible to T const*." );

    
    using cxs_here_t = detail::completions_state<
      /*EventPredicate=*/detail::event_is_here,
      /*EventValues=*/detail::rput_event_values,
      Cxs>;
    using cxs_remote_t = detail::completions_state<
      /*EventPredicate=*/detail::event_is_remote,
      /*EventValues=*/detail::rput_event_values,
      Cxs>;

 
 
    // Construct list of dest run pointers
    std::vector<void*> dst_ptrs;
    
       // Construct list of src run pointers. The old code called `resize` followed
    // by setting elements, which incurred an unnecessary zeroing of the elements
    // during the resize. This new way is to do a `reserve` followed by `push_back's`.
    dst_ptrs.reserve(std::distance(dst_runs_begin, dst_runs_end));
 
    intrank_t dst_rank = 0; // zero is a valid rank for gasnet to perform completion
    if(dst_ptrs.capacity() !=0) dst_rank = (*dst_runs_begin).rank_;
    for(DestIter d=dst_runs_begin; !(d == dst_runs_end); ++d) {
      UPCXX_ASSERT(dst_rank == (*d).rank_,
        "All global_ptr's in destination must reference memory from the same rank."
      );
      dst_rank = (*d).rank_;
      dst_ptrs.push_back((*d).raw_ptr_);
    }

    UPCXX_ASSERT(!(dst_ptrs.size()==0  && detail::completions_has_event<Cxs, remote_cx_event>::value),
                 "Cannot request remote completion without providing at least one global_ptr "
                 "in the destination sequence." );

    std::vector<void*> src_ptrs;

    src_ptrs.reserve(std::distance(src_runs_begin, src_runs_end));
  

    for(SrcIter s=src_runs_begin; !(s == src_runs_end); ++s)
      src_ptrs.push_back(const_cast<void*>((void const*)*s));

    UPCXX_ASSERT(src_ptrs.size()*src_run_length == dst_ptrs.size()*dst_run_length,
                 "Source and destination must contain same number of elements.");

    detail::rput_cbs_reg<cxs_here_t, cxs_remote_t> cbs_static{
      dst_rank,
      cxs_here_t(std::move(cxs)),
      cxs_remote_t(std::move(cxs)),
      std::move(src_ptrs), std::move(dst_ptrs)
    };

    auto *cbs = decltype(cbs_static)::static_scope
      ? &cbs_static
      : new decltype(cbs_static){std::move(cbs_static)};
    
    auto returner = detail::completions_returner<
      /*EventPredicate=*/detail::event_is_here,
      /*EventValues=*/detail::rput_event_values,
      Cxs
      >{cbs->state_here};
    
    cbs->initiate(dst_run_length*sizeof(T), src_run_length*sizeof(T));
    
    return returner();
  }
  
  template<typename SrcIter, typename DestIter,
           typename Cxs=decltype(operation_cx::as_future())>
  typename detail::completions_returner<
    /*EventPredicate=*/detail::event_is_here,
    /*EventValues=*/detail::rget_byref_event_values,
    Cxs
    >::return_t
  rget_regular(
                  SrcIter src_runs_begin, SrcIter src_runs_end,
                  std::size_t src_run_length,
                  DestIter dst_runs_begin, DestIter dst_runs_end,
                  std::size_t dst_run_length,
                  Cxs cxs=completions<future_cx<operation_cx_event>>{{}})
  {

    // Pull T out of global_ptr<T> from SrcIter
    using T = typename std::iterator_traits<SrcIter>::value_type::element_type;
    using D = typename std::iterator_traits<DestIter>::value_type;
    
    static_assert(std::is_convertible</*from*/D, /*to*/const T*>::value,
                  "Destination iterator's value type not convertible to T*." );

    static_assert(is_definitely_trivially_serializable<T>::value,
                  "RMA operations only work on DefinitelyTriviallySerializable types.");
    
    UPCXX_ASSERT((detail::completions_has_event<Cxs, operation_cx_event>::value |
                  detail::completions_has_event<Cxs, remote_cx_event>::value),
                 "Not requesting either operation or remote completion is surely an "
                 "error. You'll have know way of ever knowing when the target memory is "
                 "safe to read or write again.");
    

    static_assert( is_definitely_trivially_serializable<T>::value,
                   "RMA operations only work on DefinitelyTriviallySerializable types.");
    
    using cxs_here_t = detail::completions_state<
      /*EventPredicate=*/detail::event_is_here,
      /*EventValues=*/detail::rget_byref_event_values,
      Cxs>;
    using cxs_remote_t = detail::completions_state<
      /*EventPredicate=*/detail::event_is_remote,
      /*EventValues=*/detail::rget_byref_event_values,
      Cxs>;

    // Construct list of src run pointers. The old code called `resize` followed
    // by setting elements, which incurred an unnecessary zeroing of the elements
    // during the resize. This new way is to do a `reserve` followed by `push_back's`.
    
       // Construct list of dest run pointers
    std::vector<void*> dst_ptrs;
    dst_ptrs.reserve(std::distance(dst_runs_begin, dst_runs_end));
 
    for(DestIter d=dst_runs_begin; !(d == dst_runs_end); ++d)
      dst_ptrs.push_back((void*)*d);

    
    std::vector<void*> src_ptrs;
    src_ptrs.reserve(std::distance(src_runs_begin, src_runs_end));
   
    intrank_t src_rank = 0; // gasnet accepts rank zero for empty message
    if(src_ptrs.capacity() != 0) src_rank = (*src_runs_begin).rank_;
    for(SrcIter s=src_runs_begin; !(s == src_runs_end); ++s) {
      UPCXX_ASSERT(src_rank == (*s).rank_,
        "All global_ptr's in source runs must reference memory on the same rank."
      );
      src_ptrs.push_back((*s).raw_ptr_);
      src_rank = (*s).rank_;
    }

 
    
    UPCXX_ASSERT(
      src_ptrs.size()*src_run_length == dst_ptrs.size()*dst_run_length,
      "Source and destination runs must contain the same number of elements."
    );
    
    auto *cb = new detail::rget_cb_reg<cxs_here_t,cxs_remote_t>{
      src_rank,
      cxs_here_t{std::move(cxs)},
      cxs_remote_t{std::move(cxs)},
      std::move(src_ptrs), std::move(dst_ptrs)
    };

    auto returner = detail::completions_returner<
        /*EventPredicate=*/detail::event_is_here,
        /*EventValues=*/detail::rget_byref_event_values,
        Cxs
      >{cb->state_here};

    cb->initiate(src_rank, src_run_length*sizeof(T), dst_run_length*sizeof(T));
    
    return returner();
  }

  
  template<std::size_t Dim, typename T,
           typename Cxs=decltype(operation_cx::as_future())>
  typename detail::completions_returner<
    /*EventPredicate=*/detail::event_is_here,
    /*EventValues=*/detail::rput_event_values,
    Cxs>::return_t  
    rput_strided(
       T const *src_base,
       std::ptrdiff_t const *src_strides,
       global_ptr<T> dest_base,
       std::ptrdiff_t const *dest_strides,
       std::size_t const *extents,
       Cxs cxs=completions<future_cx<operation_cx_event>>{{}})
  {
    static_assert(
      is_definitely_trivially_serializable<T>::value,
      "RMA operations only work on DefinitelyTriviallySerializable types."
    );
    
    UPCXX_ASSERT_ALWAYS((
      detail::completions_has_event<Cxs, operation_cx_event>::value |
      detail::completions_has_event<Cxs, remote_cx_event>::value),
      "Not requesting either operation or remote completion is surely an "
      "error. You'll have know way of ever knowing when the target memory is "
      "safe to read or write again."
                         );
    
    using cxs_here_t = detail::completions_state<
      /*EventPredicate=*/detail::event_is_here,
      /*EventValues=*/detail::rput_event_values,
      Cxs>;
    using cxs_remote_t = detail::completions_state<
      /*EventPredicate=*/detail::event_is_remote,
      /*EventValues=*/detail::rput_event_values,
      Cxs>;

    detail::rput_cbs_strided<cxs_here_t, cxs_remote_t> cbs_static{
      dest_base.rank_,
      cxs_here_t{std::move(cxs)},
      cxs_remote_t{std::move(cxs)}
    };
        auto *cbs = decltype(cbs_static)::static_scope
      ? &cbs_static
      : new decltype(cbs_static){std::move(cbs_static)};
    
    auto returner = detail::completions_returner<
        /*EventPredicate=*/detail::event_is_here,
        /*EventValues=*/detail::rput_event_values,
        Cxs
      >{cbs->state_here};
    
    cbs->initiate(dest_base.rank_, dest_base.raw_ptr_, dest_strides,
                  src_base, src_strides, sizeof(T), extents, Dim);
    
    return returner();
  }

  template<std::size_t Dim, typename T,
           typename Cxs=decltype(operation_cx::as_future())>
  typename detail::completions_returner<
    /*EventPredicate=*/detail::event_is_here,
    /*EventValues=*/detail::rput_event_values,
    Cxs>::return_t  
  rput_strided(
               T const *src_base,
               std::array<std::ptrdiff_t,Dim> const &src_strides,
               global_ptr<T> dest_base,
               std::array<std::ptrdiff_t,Dim> const &dest_strides,
               std::array<std::size_t,Dim> const &extents,
               Cxs cxs=completions<future_cx<operation_cx_event>>{{}})
  {
    return rput_strided<Dim, T, Cxs>(src_base,&src_strides.front(),
                                     dest_base, &dest_strides.front(),
                                     &extents.front(), cxs);
  }
  
  template<std::size_t Dim, typename T,
           typename Cxs=decltype(operation_cx::as_future())>
  typename detail::completions_returner<
    /*EventPredicate=*/detail::event_is_here,
    /*EventValues=*/detail::rput_event_values,
    Cxs>::return_t  
  rget_strided(
               global_ptr<T> src_base,
               std::ptrdiff_t const *src_strides,
               T* dest_base,
               std::ptrdiff_t const *dest_strides,
               std::size_t const *extents,
               Cxs cxs=completions<future_cx<operation_cx_event>>{{}})
  {
    static_assert(is_definitely_trivially_serializable<T>::value,
      "RMA operations only work on DefinitelyTriviallySerializable types.");
    
    UPCXX_ASSERT((detail::completions_has_event<Cxs, operation_cx_event>::value |
                  detail::completions_has_event<Cxs, remote_cx_event>::value),
                 "Not requesting either operation or remote completion is surely an "
                 "error. You'll have know way of ever knowing when the target memory is "
                 "safe to read or write again.");
 
    using cxs_here_t = detail::completions_state<
      /*EventPredicate=*/detail::event_is_here,
      /*EventValues=*/detail::rput_event_values,
      Cxs>;
    using cxs_remote_t = detail::completions_state<
      /*EventPredicate=*/detail::event_is_remote,
      /*EventValues=*/detail::rput_event_values,
      Cxs>;

    auto *cbs = new detail::rget_cbs_strided<cxs_here_t, cxs_remote_t>{
      src_base.rank_,
      cxs_here_t{std::move(cxs)},
      cxs_remote_t{std::move(cxs)}
    };
    
    auto returner = detail::completions_returner<
      /*EventPredicate=*/detail::event_is_here,
      /*EventValues=*/detail::rput_event_values,
      Cxs
      >{cbs->state_here};
    
    cbs->initiate(dest_base, dest_strides,
                  src_base.rank_, src_base.raw_ptr_, src_strides, sizeof(T), extents, Dim);
    
    return returner();
  }
  


  template<std::size_t Dim, typename T,
           typename Cxs=decltype(operation_cx::as_future())>
  typename detail::completions_returner<
    /*EventPredicate=*/detail::event_is_here,
    /*EventValues=*/detail::rput_event_values,
    Cxs>::return_t  
  rget_strided(
               global_ptr<T> src_base,
               std::array<std::ptrdiff_t,Dim> const &src_strides,
               T *dest_base,
               std::array<std::ptrdiff_t,Dim> const &dest_strides,
               std::array<std::size_t,Dim> const &extents,
               Cxs cxs=completions<future_cx<operation_cx_event>>{{}})
  {
    return rget_strided<Dim, T, Cxs>(src_base,&src_strides.front(),
                              dest_base, &dest_strides.front(),
                              &extents.front(), cxs);
  }
 
}


#endif
