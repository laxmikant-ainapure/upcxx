#ifndef _b91be200_fd4d_41f5_a326_251161564ec7
#define _b91be200_fd4d_41f5_a326_251161564ec7

#include <upcxx/backend.hpp>
#include <upcxx/completion.hpp>
#include <upcxx/bind.hpp>
#include <upcxx/team.hpp>

namespace upcxx {
  //////////////////////////////////////////////////////////////////////////////
  // upcxx::broadcast_nontrivial
  
  template<typename T1,
           typename Cxs = completions<future_cx<operation_cx_event>>,
           typename T = typename std::decay<T1>::type>
  future<T> broadcast_nontrivial(
      T1 &&value, intrank_t root,
      team &tm = upcxx::world(),
      completions<future_cx<operation_cx_event>> cxs_ignored = {{}}
    ) {
    
    digest id = tm.next_collective_id(detail::internal_only());
    promise<T> *pro = detail::registered_promise<T>(id, /*anon=*/1);
    
    if(tm.rank_me() == root) {
      backend::bcast_am_master<progress_level::user>(
        tm,
        upcxx::bind([=](T &value) {
            promise<T> *pro = detail::registered_promise<T>(id, /*anon=*/1);
            backend::fulfill_during<progress_level::user>(*pro, std::tuple<T>(std::move(value)));
          },
          value
        )
      );
      
      backend::fulfill_during<progress_level::user>(*pro, std::tuple<T>(std::move(value)), backend::master);
    }
    
    backend::fulfill_during<progress_level::user>(*pro, /*anon*/1, backend::master);
    
    future<T> ans = pro->get_future();
    
    ans.then([=](T&) {
      delete pro;
      detail::registry.erase(id);
    });
    
    return ans;
  }
  
  //////////////////////////////////////////////////////////////////////////////
  // upcxx::broadcast
  
  namespace detail {
    // Calls GEX broadcast
    void broadcast_trivial(
      team &tm, intrank_t root, void *buf, std::size_t size,
      backend::gasnet::handle_cb *cb
    );
  }
  
  template<typename T,
           typename Cxs = completions<future_cx<operation_cx_event>>>
  future<> broadcast(
      T *buf, std::size_t n, intrank_t root,
      team &tm = upcxx::world(),
      completions<future_cx<operation_cx_event>> cxs_ignored = {{}}
    ) {
    static_assert(
      upcxx::is_definitely_trivially_serializable<T>::value,
      "Only TriviallySerializable types permitted for `upcxx::broadcast`. "
      "Consider `upcxx::broadcast_nontrivial` instead."
    );
    
    struct my_cb final: backend::gasnet::handle_cb {
      promise<> pro;
      void execute_and_delete(backend::gasnet::handle_cb_successor) override {
        backend::fulfill_during<progress_level::user>(std::move(pro), /*anon*/1, backend::master);
        delete this;
      }
    };
    
    my_cb *cb = new my_cb;
    future<> ans = cb->pro.get_future();
    detail::broadcast_trivial(tm, root, (void*)buf, n*sizeof(T), cb);
    return ans;
  }
  
  template<typename T,
           typename Cxs = completions<future_cx<operation_cx_event>>>
  future<T> broadcast(
      T value, intrank_t root,
      team &tm = upcxx::world(),
      completions<future_cx<operation_cx_event>> cxs_ignored = {{}}
    ) {
    
    static_assert(
      upcxx::is_definitely_trivially_serializable<T>::value,
      "Only TriviallySerializable types permitted for `upcxx::broadcast`. "
      "Consider `upcxx::broadcast_nontrivial` instead."
    );
    
    struct my_cb final: backend::gasnet::handle_cb {
      promise<T> pro;
      T val;
      my_cb(T val): val(std::move(val)) {}
      void execute_and_delete(backend::gasnet::handle_cb_successor) override {
        backend::fulfill_during<progress_level::user>(std::move(pro), std::tuple<T>(std::move(val)), backend::master);
        delete this;
      }
    };
    
    my_cb *cb = new my_cb(std::move(value));
    future<T> ans = cb->pro.get_future();
    detail::broadcast_trivial(tm, root, (void*)&cb->val, sizeof(T), cb);
    return ans;
  }
}
#endif
