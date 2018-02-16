#ifndef _CECA99E6_CB27_41E2_9478_E2A9B106BBF2
#define _CECA99E6_CB27_41E2_9478_E2A9B106BBF2

#include <upcxx/completion.hpp>
#include <upcxx/global_ptr.hpp>

//  I'm not sure we will have a Vector-Index-Strided interface that is
//  not GASNet-based, so if an application wants to make use of the VIS
//  interface it is likely they will want to be running GASNet-Ex 
#include <upcxx/backend/gasnet/runtime.hpp>

namespace upcxx
{

//  for pass one through the implementation the VIS functions will return
//  a void future<> which represents global completion.  The RType gets more
//  involved later when we infer the RType from the Completions argument

  template<typename SrcIter, typename DestIter,
         typename Completions=decltype(operation_cx::as_future())>
  future<>  rput_fragmented(
        SrcIter src_runs_begin, SrcIter src_runs_end,
        DestIter dest_runs_begin, DestIter dest_runs_end,
        Completions cxs=Completions{});

  template<typename SrcIter, typename DestIter,
         typename Completions=decltype(operation_cx::as_future())>
  future<>  rget_fragmented(
       SrcIter src_runs_begin, SrcIter src_runs_end,
       DestIter dest_runs_begin, DestIter dest_runs_end,
       Completions cxs=Completions{});

  template<typename SrcIter, typename DestIter,
         typename Completions=decltype(operation_cx::as_future())>
  future<>  rput_fragmented_regular(
       SrcIter src_runs_begin, SrcIter src_runs_end,
       std::size_t src_run_length,
       DestIter dest_runs_begin, DestIter dest_runs_end,
       std::size_t dest_run_length,
       Completions cxs=Completions{});

  template<typename SrcIter, typename DestIter,
         typename Completions=decltype(operation_cx::as_future())>
  future<> rget_fragmented_regular(
       SrcIter src_runs_begin, SrcIter src_runs_end,
       std::size_t src_run_length,
       DestIter dest_runs_begin, DestIter dest_runs_end,
       std::size_t dest_run_length,
       Completions cxs=Completions{});

  template<std::size_t Dim, typename T,
         typename Completions=decltype(operation_cx::as_future())>
  future<>  rput_strided(
       T const *src_base,
       std::ptrdiff_t const *src_strides,
       global_ptr<T> dest_base,
       std::ptrdiff_t const *dest_strides,
       std::size_t const *extents,
       Completions cxs=Completions{});

  template<std::size_t Dim, typename T,
         typename Completions=decltype(operation_cx::as_future())>
  future<> rput_strided(
       T const *src_base,
       std::array<std::ptrdiff_t,Dim> const &src_strides,
       global_ptr<T> dest_base,
       std::array<std::ptrdiff_t,Dim> const &dest_strides,
       std::array<std::size_t,Dim> const &extents,
       Completions cxs=Completions{});

  template<std::size_t Dim, typename T,
         typename Completions=decltype(operation_cx::as_future())>
  future<> rget_strided(
       global_ptr<T> src_base,
       std::ptrdiff_t const *src_strides,
       T *dest_base,
       std::ptrdiff_t const *dest_strides,
       std::size_t const *extents,
       Completions cxs=Completions{});

  template<std::size_t Dim, typename T,
         typename Completions=decltype(operation_cx::as_future())>
  future<>  rget_strided(
       global_ptr<T> src_base,
       std::array<std::ptrdiff_t,Dim> const &src_strides,
       T *dest_base,
       std::array<std::ptrdiff_t,Dim> const &dest_strides,
       std::array<std::size_t,Dim> const &extents,
       Completions cxs=Completions{});
}


#endif
