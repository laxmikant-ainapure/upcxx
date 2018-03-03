#ifndef _CECA99E6_CB27_41E2_9478_E2A9B106BBF2
#define _CECA99E6_CB27_41E2_9478_E2A9B106BBF2

#include <upcxx/completion.hpp>
#include <upcxx/global_ptr.hpp>
#include <upcxx/rput.hpp>
#include <upcxx/rget.hpp>
#include <type_traits>

//  I'm not sure we will have a Vector-Index-Strided interface that is
//  not GASNet-based, so if an application wants to make use of the VIS
//  interface it is likely they will want to be running GASNet-Ex 
#include <upcxx/backend/gasnet/runtime.hpp>

namespace upcxx
{

  namespace detail {

      
    void rma_put_irreg_nb(
                         intrank_t rank_d,
                         std::size_t _dstcount,
                         upcxx::backend::memvec_t const _dstlist[],
                         std::size_t _srcount,
                         upcxx::backend::memvec_t const _srclist[],
                         backend::gasnet::handle_cb *source_cb,
                         backend::gasnet::handle_cb *operation_cb);
    void rma_get_irreg_nb(                               
                         std::size_t _dstcount,
                         upcxx::backend::memvec_t const _dstlist[],
                         upcxx::intrank_t rank_s,
                         std::size_t _srccount,
                         upcxx::backend::memvec_t const _srclist[],
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
      std::vector<upcxx::backend::memvec_t> src;
      std::vector<upcxx::backend::memvec_t> dest;
      rput_cbs_irreg(intrank_t rank_d, CxStateHere here, CxStateRemote remote,
                    std::vector<upcxx::backend::memvec_t>&& src,
                    std::vector<upcxx::backend::memvec_t>&& dest):
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
      std::vector<upcxx::backend::memvec_t> src;
      std::vector<upcxx::backend::memvec_t> dest;
      rget_cb_irreg(intrank_t rank_s, CxStateHere here, CxStateRemote remote,
                   std::vector<upcxx::backend::memvec_t>&& Src,
                   std::vector<upcxx::backend::memvec_t>&& Dest)
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
    UPCXX_ASSERT_ALWAYS((detail::completions_has_event<Cxs, operation_cx_event>::value));
    // can't seem to get this assert to work right.
    //static_assert(std::is_same<decltype(std::get<0>(*src_runs_begin)), decltype(std::get<0>(*dst_runs_begin).raw_ptr_)>::value, "SrcIter and DestIter need to be over same base T type");
 
                 
    using cxs_here_t = detail::completions_state<
      /*EventPredicate=*/detail::event_is_here,
      /*EventValues=*/detail::rput_event_values,
      Cxs>;
    using cxs_remote_t = detail::completions_state<
      /*EventPredicate=*/detail::event_is_remote,
      /*EventValues=*/detail::rput_event_values,
      Cxs>;

    std::vector<upcxx::backend::memvec_t>  src, dest;
    std::size_t srccount=0;
    std::size_t srcsize=0;
    std::size_t dstcount=0;
    std::size_t dstsize=0;
    constexpr std::size_t tsize=sizeof(*std::get<0>(*src_runs_begin));
    srccount = std::distance(src_runs_begin, src_runs_end);
    dstcount = std::distance(dst_runs_begin, dst_runs_end);
 
    src.resize(srccount);
    dest.resize(dstcount);
    auto sv=src.begin();
    auto dv=dest.begin();
    for(SrcIter s=src_runs_begin; !(s==src_runs_end); ++s,++sv)
      {
        sv->gex_addr=std::get<0>(*s);
        sv->gex_len =std::get<1>(*s)*tsize;
        srcsize+=sv->gex_len;
      }
    intrank_t gpdrank = (std::get<0>(*dst_runs_begin)).rank_;
    for(DestIter d=dst_runs_begin; !(d==dst_runs_end); ++d,++dv)
      {
        UPCXX_ASSERT(gpdrank==std::get<0>(*d).rank_);
        dv->gex_addr=(std::get<0>(*d)).raw_ptr_;
        dv->gex_len =std::get<1>(*d)*tsize;
        dstsize+=dv->gex_len;
      }
    
    UPCXX_ASSERT_ALWAYS(dstsize==srcsize);
    
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
    UPCXX_ASSERT_ALWAYS((detail::completions_has_event<Cxs, operation_cx_event>::value));
    //static_assert(std::is_same<decltype((std::get<0>(*src_runs_begin)).raw_ptr_),decltype(std::get<0>(*dst_runs_begin))>::value, "type mismatch");
    using cxs_here_t = detail::completions_state<
      /*EventPredicate=*/detail::event_is_here,
      /*EventValues=*/detail::rget_byref_event_values,
      Cxs>;
    using cxs_remote_t = detail::completions_state<
      /*EventPredicate=*/detail::event_is_remote,
      /*EventValues=*/detail::rget_byref_event_values,
      Cxs>;

    intrank_t rank_s = std::get<0>(*src_runs_begin).rank_;
    std::size_t srccount=0;
    std::size_t srcsize=0;
    std::size_t dstcount=0;
    std::size_t dstsize=0;
    constexpr std::size_t tsize=sizeof(*std::get<0>(*dst_runs_begin));
    srccount = std::distance(src_runs_begin, src_runs_end);
    dstcount = std::distance(dst_runs_begin, dst_runs_end);
    std::vector<upcxx::backend::memvec_t> src(srccount), dest(dstcount);
    auto sv=src.begin();
    auto dv=dest.begin();
    for(SrcIter s=src_runs_begin; !(s==src_runs_end); ++s,++sv)
      {
        UPCXX_ASSERT(rank_s==std::get<0>(*s).rank_);
        sv->gex_addr=std::get<0>(*s).raw_ptr_;
        sv->gex_len =std::get<1>(*s)*tsize;
        srcsize+=sv->gex_len;
      }

    for(DestIter d=dst_runs_begin; !(d==dst_runs_end); ++d,++dv)
      {
        dv->gex_addr=(std::get<0>(*d));
        dv->gex_len =std::get<1>(*d)*tsize;
        dstsize+=dv->gex_len;
      }
    
    UPCXX_ASSERT_ALWAYS(dstsize==srcsize);
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

    UPCXX_ASSERT_ALWAYS((detail::completions_has_event<Cxs, operation_cx_event>::value));
    static_assert(std::is_same<decltype(*src_runs_begin),decltype((*dst_runs_begin).raw_ptr_)>::value, "type mismatch");
    using cxs_here_t = detail::completions_state<
      /*EventPredicate=*/detail::event_is_here,
      /*EventValues=*/detail::rput_event_values,
      Cxs>;
    using cxs_remote_t = detail::completions_state<
      /*EventPredicate=*/detail::event_is_remote,
      /*EventValues=*/detail::rput_event_values,
      Cxs>;

    std::vector<void*>  src, dest;
    std::size_t srccount=0;
    std::size_t dstcount=0;
    constexpr std::size_t srcSize=sizeof(*src_runs_begin);
    constexpr std::size_t dstSize=sizeof(*(*dst_runs_begin).raw_ptr_);
    srccount = std::distance(src_runs_begin, src_runs_end);
    dstcount = std::distance(dst_runs_begin, dst_runs_end);
    UPCXX_ASSERT_ALWAYS(dstcount*dst_run_length*dstSize==srccount*src_run_length*srcSize);
    
    src.resize(srccount);
    dest.resize(dstcount);
    auto sv=src.begin();
    auto dv=dest.begin();
    for(SrcIter s=src_runs_begin; !(s==src_runs_end); ++s,++sv)
      {
        *sv = *s;
      }
    intrank_t gpdrank = (*dst_runs_begin).rank_;
    for(DestIter d=dst_runs_begin; !(d==dst_runs_end); ++d,++dv)
      {
        UPCXX_ASSERT((*d).rank_==gpdrank);
        *dv=(*d).raw_ptr_;
      }
    detail::rput_cbs_reg<cxs_here_t, cxs_remote_t> cbs_static{
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
    
    cbs->initiate(dst_run_length*dstSize, src_run_length*srcSize);
    
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
    UPCXX_ASSERT_ALWAYS((detail::completions_has_event<Cxs, operation_cx_event>::value));
    static_assert(std::is_same<decltype((*src_runs_begin).raw_ptr_),decltype((*dst_runs_begin))>::value, "type mismatch");
    using cxs_here_t = detail::completions_state<
      /*EventPredicate=*/detail::event_is_here,
      /*EventValues=*/detail::rget_byref_event_values,
      Cxs>;
    using cxs_remote_t = detail::completions_state<
      /*EventPredicate=*/detail::event_is_remote,
      /*EventValues=*/detail::rget_byref_event_values,
      Cxs>;

    intrank_t rank_s = (*src_runs_begin).rank_;
    std::size_t srccount=0;
    std::size_t dstcount=0;
    constexpr std::size_t tsize=sizeof(*dst_runs_begin);
    srccount = std::distance(src_runs_begin, src_runs_end);
    dstcount = std::distance(dst_runs_begin, dst_runs_end);
    UPCXX_ASSERT(srccount*src_run_length==dstcount*dst_run_length);
    
    std::vector<void*> src(srccount), dest(dstcount);
    auto sv=src.begin();
    auto dv=dest.begin();
    for(SrcIter s=src_runs_begin; !(s==src_runs_end); ++s,++sv)
      {
        UPCXX_ASSERT((*s).rank_);
        *sv = (*s).raw_ptr_;
      }

    for(DestIter d=dst_runs_begin; !(d==dst_runs_end); ++d,++dv)
      {
        *dv= *d;
      }
    
    auto *cb = new detail::rget_cb_reg<cxs_here_t,cxs_remote_t>{
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

    cb->initiate(rank_s, src_run_length*tsize, dst_run_length*tsize);
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
    UPCXX_ASSERT_ALWAYS((detail::completions_has_event<Cxs, operation_cx_event>::value));
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
    UPCXX_ASSERT_ALWAYS((detail::completions_has_event<Cxs, operation_cx_event>::value));
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
