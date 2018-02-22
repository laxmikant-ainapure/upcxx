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

  
  template<typename SrcIter, typename DestIter,
           typename Cxs=decltype(operation_cx::as_future())>
  typename detail::completions_returner<
    /*EventPredicate=*/detail::event_is_here,
    /*EventValues=*/detail::rput_event_values,
    Cxs>::return_t  
  rput_fragmented(
                  SrcIter src_runs_begin, SrcIter src_runs_end,
                  DestIter dest_runs_begin, DestIter dest_runs_end,
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

    intrank_t gpdrank = (std::get<0>(*dest_runs_begin)).rank_;
    detail::rput_cbs_byref<cxs_here_t, cxs_remote_t> cbs_static{
      gpdrank,
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
    
    //cbs->initiate(gp_d.rank_, gp_d.raw_ptr_, buf_s, n*sizeof(T));
    
    return returner();
  }

template<typename SrcIter, typename DestIter>
           //         typename Completions=decltype(operation_cx::as_future())>
  future<>  rget_fragmented(
       SrcIter src_runs_begin, SrcIter src_runs_end,
         DestIter dest_runs_begin, DestIter dest_runs_end);
//       Completions cxs=Completions{});

  template<typename SrcIter, typename DestIter>
//         typename Completions=decltype(operation_cx::as_future())>
  future<>  rput_regular(
       SrcIter src_runs_begin, SrcIter src_runs_end,
       std::size_t src_run_length,
       DestIter dest_runs_begin, DestIter dest_runs_end,
         std::size_t dest_run_length);
       //       Completions cxs=Completions{});

template<typename SrcIter, typename DestIter>
           //         typename Completions=decltype(operation_cx::as_future())>
  future<> rget_regular(
       SrcIter src_runs_begin, SrcIter src_runs_end,
       std::size_t src_run_length,
       DestIter dest_runs_begin, DestIter dest_runs_end,
         std::size_t dest_run_length);
       //       Completions cxs=Completions{});

template<std::size_t Dim, typename T>
           //        typename Completions=decltype(operation_cx::as_future())>
  future<>  rput_strided(
       T const *src_base,
       std::ptrdiff_t const *src_strides,
       global_ptr<T> dest_base,
       std::ptrdiff_t const *dest_strides,
         std::size_t const *extents);
       //      Completions cxs=Completions{});

template<std::size_t Dim, typename T>
           //        typename Completions=decltype(operation_cx::as_future())>
  future<> rput_strided(
       T const *src_base,
       std::array<std::ptrdiff_t,Dim> const &src_strides,
       global_ptr<T> dest_base,
       std::array<std::ptrdiff_t,Dim> const &dest_strides,
         std::array<std::size_t,Dim> const &extents);
       //       Completions cxs=Completions{});

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
                              void *_srcaddr, const std::ptrdiff_t _srcstrides[],
                              std::size_t _elemsz,
                              const std::size_t _count[], std::size_t _stridelevels,
                              backend::gasnet::handle_cb *source_cb,
                              backend::gasnet::handle_cb *operation_cb);
    }
}


#endif
