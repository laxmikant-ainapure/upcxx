#ifndef _CECA99E6_CB27_41E2_9478_E2A9B106BBF2
#define _CECA99E6_CB27_41E2_9478_E2A9B106BBF2

#include <upcxx/completion.hpp>
#include <upcxx/global_ptr.hpp>
#include <upcxx/rput.hpp>

//  I'm not sure we will have a Vector-Index-Strided interface that is
//  not GASNet-based, so if an application wants to make use of the VIS
//  interface it is likely they will want to be running GASNet-Ex 
#include <upcxx/backend/gasnet/runtime.hpp>

namespace upcxx
{

  namespace detail {

      
    void rma_put_frag_nb(
                         intrank_t rank_d,
                         std::size_t _dstcount,
                         upcxx::backend::memvec_t const _dstlist[],
                         std::size_t _srcount,
                         upcxx::backend::memvec_t const _srclist[],
                         backend::gasnet::handle_cb *source_cb,
                         backend::gasnet::handle_cb *operation_cb);
    
    void rma_put_reg_nb(
                        intrank_t rank_d,
                        size_t _dstcount, void * const _dstlist[], size_t _dstlen,
                        size_t _srccount, void * const _srclist[], size_t _srclen,
                        backend::gasnet::handle_cb *source_cb,
                        backend::gasnet::handle_cb *operation_cb);
    
    void rma_put_strided_nb(
                            intrank_t rank_d,
                            void *_dstaddr, const std::ptrdiff_t _dststrides[],
                            const void *_srcaddr, const std::ptrdiff_t _srcstrides[],
                            std::size_t _elemsz,
                            const std::size_t _count[], std::size_t _stridelevels,
                            backend::gasnet::handle_cb *source_cb,
                            backend::gasnet::handle_cb *operation_cb);
    

    template<typename CxStateHere, typename CxStateRemote>
    struct rput_cbs_frag final:
      rput_cb_source</*FinalType=*/rput_cbs_frag<CxStateHere, CxStateRemote>,
                                   CxStateHere, CxStateRemote>,
      rput_cb_operation</*FinalType=*/rput_cbs_frag<CxStateHere, CxStateRemote>,
                                      CxStateHere, CxStateRemote>
    {
      intrank_t rank_d;
      CxStateHere state_here;
      CxStateRemote state_remote;
      std::vector<upcxx::backend::memvec_t> src;
      std::vector<upcxx::backend::memvec_t> dest;
      rput_cbs_frag(intrank_t rank_d, CxStateHere here, CxStateRemote remote,
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
        detail::rma_put_frag_nb(rank_d, dest.size(), &(dest[0]),
                                src.size(), &(src[0]),
                                this->source_cb(), this);
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
                               this->source_cb(), this);
      }

    };
    
    template<typename CxStateHere, typename CxStateRemote>
    struct rput_cbs_stride final:
      rput_cb_source</*FinalType=*/rput_cbs_stride<CxStateHere, CxStateRemote>,
                                   CxStateHere, CxStateRemote>,
      rput_cb_operation</*FinalType=*/rput_cbs_stride<CxStateHere, CxStateRemote>,
                                      CxStateHere, CxStateRemote>
    {
      intrank_t rank_d;
      CxStateHere state_here;
      CxStateRemote state_remote;
      rput_cbs_stride(intrank_t rank_d, CxStateHere here, CxStateRemote remote):
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
                                  this->source_cb(), this);
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
  rput_fragmented(
                  SrcIter src_runs_begin, SrcIter src_runs_end,
                  DestIter dst_runs_begin, DestIter dst_runs_end,
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

    std::vector<upcxx::backend::memvec_t>  src, dest;
    std::size_t srccount=0;
    std::size_t srcsize=0;
    std::size_t dstcount=0;
    std::size_t dstsize=0;
    constexpr std::size_t srcSize=sizeof(*std::get<0>(*src_runs_begin));
    constexpr std::size_t dstSize=sizeof(*std::get<0>(*dst_runs_begin).raw_ptr_);
    srccount = std::distance(src_runs_begin, src_runs_end);
    dstcount = std::distance(dst_runs_begin, dst_runs_end);
 
    src.resize(srccount);
    dest.resize(dstcount);
    auto sv=src.begin();
    auto dv=dest.begin();
    for(SrcIter s=src_runs_begin; !(s==src_runs_end); ++s,++sv)
      {
        sv->gex_addr=std::get<0>(*s);
        sv->gex_len =std::get<1>(*s)*srcSize;
        srcsize+=sv->gex_len;
      }
    intrank_t gpdrank = (std::get<0>(*dst_runs_begin)).rank_;
    for(DestIter d=dst_runs_begin; !(d==dst_runs_end); ++d,++dv)
      {
        UPCXX_ASSERT(gpdrank==std::get<0>(*d).rank_);
        dv->gex_addr=(std::get<0>(*d)).raw_ptr_;
        dv->gex_len =std::get<1>(*d)*dstSize;
        dstsize+=dv->gex_len;
      }
    
    UPCXX_ASSERT_ALWAYS(dstsize==srcsize);
    
    detail::rput_cbs_frag<cxs_here_t, cxs_remote_t> cbs_static{
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

template<typename SrcIter, typename DestIter>
           //         typename Completions=decltype(operation_cx::as_future())>
  future<>  rget_fragmented(
       SrcIter src_runs_begin, SrcIter src_runs_end,
         DestIter dest_runs_begin, DestIter dest_runs_end);
//       Completions cxs=Completions{});

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

template<typename SrcIter, typename DestIter>
           //         typename Completions=decltype(operation_cx::as_future())>
  future<> rget_regular(
       SrcIter src_runs_begin, SrcIter src_runs_end,
       std::size_t src_run_length,
       DestIter dest_runs_begin, DestIter dest_runs_end,
         std::size_t dest_run_length);
       //       Completions cxs=Completions{});
  
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

    detail::rput_cbs_stride<cxs_here_t, cxs_remote_t> cbs_static{
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

template<std::size_t Dim, typename T>
           //         typename Completions=decltype(operation_cx::as_future())>
  future<> rget_strided(
       global_ptr<T> src_base,
       std::ptrdiff_t const *src_strides,
       T *dest_base,
       std::ptrdiff_t const *dest_strides,
         std::size_t const *extents);
       //       Completions cxs=Completions{});

template<std::size_t Dim, typename T>
           //         typename Completions=decltype(operation_cx::as_future())>
  future<>  rget_strided(
       global_ptr<T> src_base,
       std::array<std::ptrdiff_t,Dim> const &src_strides,
       T *dest_base,
       std::array<std::ptrdiff_t,Dim> const &dest_strides,
         std::array<std::size_t,Dim> const &extents);
       //       Completions cxs=Completions{});

 
}


#endif
