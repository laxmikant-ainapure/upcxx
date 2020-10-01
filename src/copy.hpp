#ifndef _f42075e8_08a8_4472_8972_3919ea92e6ff
#define _f42075e8_08a8_4472_8972_3919ea92e6ff

#include <upcxx/backend.hpp>
#include <upcxx/cuda.hpp>
#include <upcxx/completion.hpp>
#include <upcxx/global_ptr.hpp>
#include <upcxx/rput.hpp>

#include <functional>

namespace upcxx {
  namespace detail {
    void rma_copy_get(void *buf_d, intrank_t rank_s, void const *buf_s, std::size_t size, backend::gasnet::handle_cb *cb);
    void rma_copy_put(intrank_t rank_d, void *buf_d, void const *buf_s, std::size_t size, backend::gasnet::handle_cb *cb);
    void rma_copy_local(
        int heap_d, void *buf_d,
        int heap_s, void const *buf_s, std::size_t size,
        cuda::event_cb *cb
      );

    constexpr int host_heap = 0;

    template<typename Cxs>
    typename detail::completions_returner<
      /*EventPredicate=*/detail::event_is_here,
      /*EventValues=*/detail::rput_event_values,
      typename std::decay<Cxs>::type
    >::return_t
    copy(const int heap_s, const intrank_t rank_s, void *const buf_s,
         const int heap_d, const intrank_t rank_d, void *const buf_d,
         const std::size_t size, Cxs &&cxs);
  }
  
  template<typename T, memory_kind Ks,
           typename Cxs = completions<future_cx<operation_cx_event>>>
  UPCXX_NODISCARD
  inline
  typename detail::completions_returner<
      /*EventPredicate=*/detail::event_is_here,
      /*EventValues=*/detail::rput_event_values,
    typename std::decay<Cxs>::type
    >::return_t
  copy(global_ptr<const T,Ks> src, T *dest, std::size_t n,
       Cxs &&cxs=completions<future_cx<operation_cx_event>>{{}}) {
    UPCXX_ASSERT_INIT();
    UPCXX_GPTR_CHK(src);
    UPCXX_ASSERT(src && dest, "pointer arguments to copy may not be null");
    return detail::copy( src.heap_idx_, src.rank_, src.raw_ptr_,
                         detail::host_heap, upcxx::rank_me(), dest,
                         n * sizeof(T), std::forward<Cxs>(cxs) );
  }

  template<typename T, memory_kind Kd,
           typename Cxs = completions<future_cx<operation_cx_event>>>
  UPCXX_NODISCARD
  inline
  typename detail::completions_returner<
      /*EventPredicate=*/detail::event_is_here,
      /*EventValues=*/detail::rput_event_values,
      typename std::decay<Cxs>::type
    >::return_t
  copy(T const *src, global_ptr<T,Kd> dest, std::size_t n,
       Cxs &&cxs=completions<future_cx<operation_cx_event>>{{}}) {
    UPCXX_ASSERT_INIT();
    UPCXX_GPTR_CHK(dest);
    UPCXX_ASSERT(src && dest, "pointer arguments to copy may not be null");
    return detail::copy( detail::host_heap, upcxx::rank_me(), const_cast<T*>(src),
                         dest.heap_idx_, dest.rank_, dest.raw_ptr_,
                         n * sizeof(T), std::forward<Cxs>(cxs) );
  }
  
  template<typename T, memory_kind Ks, memory_kind Kd,
           typename Cxs = completions<future_cx<operation_cx_event>>>
  UPCXX_NODISCARD
  inline
  typename detail::completions_returner<
      /*EventPredicate=*/detail::event_is_here,
      /*EventValues=*/detail::rput_event_values,
      typename std::decay<Cxs>::type
    >::return_t
  copy(global_ptr<const T,Ks> src, global_ptr<T,Kd> dest, std::size_t n,
       Cxs &&cxs=completions<future_cx<operation_cx_event>>{{}}) {
    UPCXX_ASSERT_INIT();
    UPCXX_GPTR_CHK(src); UPCXX_GPTR_CHK(dest);
    UPCXX_ASSERT(src && dest, "pointer arguments to copy may not be null");
    return detail::copy(
      src.heap_idx_, src.rank_, src.raw_ptr_,
      dest.heap_idx_, dest.rank_, dest.raw_ptr_,
      n*sizeof(T), std::forward<Cxs>(cxs)
    );
  }

 namespace detail {
  template<typename Cxs>
  typename detail::completions_returner<
      /*EventPredicate=*/detail::event_is_here,
      /*EventValues=*/detail::rput_event_values,
      typename std::decay<Cxs>::type
  >::return_t
  copy(const int heap_s, const intrank_t rank_s, void *const buf_s,
       const int heap_d, const intrank_t rank_d, void *const buf_d,
       const std::size_t size, Cxs &&cxs) {
    
    using CxsDecayed = typename std::decay<Cxs>::type;
    using cxs_here_t = detail::completions_state<
      /*EventPredicate=*/detail::event_is_here,
      /*EventValues=*/detail::rput_event_values,
      CxsDecayed>;
    using cxs_remote_t = detail::completions_state<
      /*EventPredicate=*/detail::event_is_remote,
      /*EventValues=*/detail::rput_event_values,
      CxsDecayed>;

    cxs_here_t *cxs_here = new cxs_here_t(std::forward<Cxs>(cxs));
    cxs_remote_t cxs_remote(std::forward<Cxs>(cxs));

    persona *initiator_per = &upcxx::current_persona();
    
    auto returner = detail::completions_returner<
        /*EventPredicate=*/detail::event_is_here,
        /*EventValues=*/detail::rput_event_values,
        CxsDecayed
      >(*cxs_here);

    if(upcxx::rank_me() != rank_d && upcxx::rank_me() != rank_s) {
      int initiator = upcxx::rank_me();
      
      backend::send_am_master<progress_level::internal>(
        upcxx::world(), rank_d,
        [=]() {
          auto operation_cx_as_internal_future = upcxx::completions<upcxx::future_cx<upcxx::operation_cx_event, progress_level::internal>>{{}};
          
          detail::copy( heap_s, rank_s, buf_s,
                        heap_d, rank_d, buf_d,
                        size, operation_cx_as_internal_future )
          .then([=]() {
            const_cast<cxs_remote_t&>(cxs_remote).template operator()<remote_cx_event>();
            
            backend::send_am_persona<progress_level::internal>(
              upcxx::world(), initiator, initiator_per,
              [=]() {
                cxs_here->template operator()<source_cx_event>();
                cxs_here->template operator()<operation_cx_event>();
                delete cxs_here;
              }
            );
          });
        }
      );
    }
    else if(rank_d == rank_s) {
      detail::rma_copy_local(heap_d, buf_d, heap_s, buf_s, size,
        cuda::make_event_cb([=]() {
          cxs_here->template operator()<source_cx_event>();
          cxs_here->template operator()<operation_cx_event>();
          const_cast<cxs_remote_t&>(cxs_remote).template operator()<remote_cx_event>();
          delete cxs_here;
        })
      );
    }
    else if(rank_d == upcxx::rank_me()) {
      cxs_remote_t *cxs_remote_heaped = new cxs_remote_t(std::move(cxs_remote));
      
      /* We are the destination, so semantically like a GET, even though a PUT
       * is used to transfer on the network
       */
      void *bounce_d;
      if(heap_d == host_heap)
        bounce_d = buf_d;
      else {
        bounce_d = backend::gasnet::allocate(size, 64, &backend::gasnet::sheap_footprint_rdzv);
      }
      
      backend::send_am_master<progress_level::internal>(
        upcxx::world(), rank_s,
        [=]() {
          auto make_bounce_s_cont = [=](void *bounce_s) {
            return [=]() {
              detail::rma_copy_put(rank_d, bounce_d, bounce_s, size,
              backend::gasnet::make_handle_cb([=]() {
                  if(heap_s != host_heap)
                    backend::gasnet::deallocate(bounce_s, &backend::gasnet::sheap_footprint_rdzv);
                  
                  backend::send_am_persona<progress_level::internal>(
                    upcxx::world(), rank_d, initiator_per,
                    [=]() {
                      cxs_here->template operator()<source_cx_event>();
                      
                      auto bounce_d_cont = [=]() {
                        if(heap_d != host_heap)
                          backend::gasnet::deallocate(bounce_d, &backend::gasnet::sheap_footprint_rdzv);

                        cxs_remote_heaped->template operator()<remote_cx_event>();
                        cxs_here->template operator()<operation_cx_event>();
                        delete cxs_remote_heaped;
                        delete cxs_here;
                      };
                      
                      if(heap_d == host_heap)
                        bounce_d_cont();
                      else
                        detail::rma_copy_local(heap_d, buf_d, host_heap, bounce_d, size, cuda::make_event_cb(bounce_d_cont));
                    }
                  );
                })
              );
            };
          };
          
          if(heap_s == host_heap)
            make_bounce_s_cont(buf_s)();
          else {
            void *bounce_s = backend::gasnet::allocate(size, 64, &backend::gasnet::sheap_footprint_rdzv);
            
            detail::rma_copy_local(
              host_heap, bounce_s, heap_s, buf_s, size,
              cuda::make_event_cb(make_bounce_s_cont(bounce_s))
            );
          }
        }
      );
    }
    else {
      /* We are the source, so semantically this is a PUT even though we use a
       * GET to transfer over network.
       */
      auto make_bounce_s_cont = [&](void *bounce_s) {
        return [=]() {
          if(heap_s != host_heap) {
            // since source side has a bounce buffer, we can signal source_cx as soon
            // as its populated
            cxs_here->template operator()<source_cx_event>();
          }
          
          backend::send_am_master<progress_level::internal>(
            upcxx::world(), rank_d,
            upcxx::bind(
              [=](cxs_remote_t &&cxs_remote) {
                void *bounce_d = heap_d == host_heap ? buf_d : backend::gasnet::allocate(size, 64, &backend::gasnet::sheap_footprint_rdzv);
                
                detail::rma_copy_get(bounce_d, rank_s, bounce_s, size,
                  backend::gasnet::make_handle_cb([=]() {
                    auto bounce_d_cont = [=]() {
                      if(heap_d != host_heap)
                        backend::gasnet::deallocate(bounce_d, &backend::gasnet::sheap_footprint_rdzv);
                      
                      const_cast<cxs_remote_t&>(cxs_remote).template operator()<remote_cx_event>();

                      backend::send_am_persona<progress_level::internal>(
                        upcxx::world(), rank_s, initiator_per,
                        [=]() {
                          if(heap_s != host_heap)
                            backend::gasnet::deallocate(bounce_s, &backend::gasnet::sheap_footprint_rdzv);
                          else {
                            // source didnt use bounce buffer, need to source_cx now
                            cxs_here->template operator()<source_cx_event>();
                          }
                          cxs_here->template operator()<operation_cx_event>();
                          
                          delete cxs_here;
                        }
                      );
                    };
                    
                    if(heap_d == host_heap)
                      bounce_d_cont();
                    else
                      detail::rma_copy_local(heap_d, buf_d, host_heap, bounce_d, size, cuda::make_event_cb(bounce_d_cont));
                  })
                );
              }, std::move(cxs_remote)
            )
          );
        };
      };

      if(heap_s == host_heap)
        make_bounce_s_cont(buf_s)();
      else {
        void *bounce_s = backend::gasnet::allocate(size, 64, &backend::gasnet::sheap_footprint_rdzv);
        
        detail::rma_copy_local(host_heap, bounce_s, heap_s, buf_s, size, cuda::make_event_cb(make_bounce_s_cont(bounce_s)));
      }
    }

    return returner();
  }
 } // namespace detail
}
#endif
