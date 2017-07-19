#ifndef _9a741fd1_dcc1_4fe9_829f_ae86fa6b86ab
#define _9a741fd1_dcc1_4fe9_829f_ae86fa6b86ab

#include <upcxx/future/core.hpp>

namespace upcxx {
  //////////////////////////////////////////////////////////////////////
  // future_is_trivially_ready: future_impl_shref specialization
  
  template<typename HeaderOps, typename ...T>
  struct future_is_trivially_ready<
      future1<detail::future_kind_shref<HeaderOps>, T...>
    > {
    static constexpr bool value = HeaderOps::is_trivially_ready_result;
  };
  
  namespace detail {
    ////////////////////////////////////////////////////////////////////
    // future_impl_shref: Future implementation using ref-counted
    // pointer to header.
    
    template<typename HeaderOps, typename ...T>
    struct future_impl_shref {
      future_header *hdr_;
    
    public:
      future_impl_shref() {
        this->hdr_ = &future_header::the_nil;
      }
      // hdr comes comes with a reference included for us
      template<typename Header>
      future_impl_shref(Header *hdr) {
        this->hdr_ = hdr;
      }
      
      future_impl_shref(const future_impl_shref &that) {
        this->hdr_ = that.hdr_;
        this->hdr_->refs_add(1);
      }
      template<typename Ops1>
      future_impl_shref(
          const future_impl_shref<Ops1,T...> &that,
          typename std::enable_if<std::is_base_of<HeaderOps,Ops1>::value>::type* = 0
        ) {
        this->hdr_ = that.hdr_;
        this->hdr_->refs_add(1);
      }
      
      future_impl_shref& operator=(const future_impl_shref &that) {
        future_header *this_hdr = this->hdr_;
        future_header *that_hdr = that.hdr_;
        
        int this_refs = this_hdr->ref_n_;
        int this_unit = this_refs < 0 || this == &that ? 0 : 1;
        
        int that_refs = that_hdr->ref_n_;
        int that_unit = that_refs < 0 || this == &that ? 0 : 1;
        
        that_refs += that_unit;
        this_refs -= this_unit;
        
        {int trash; (this_unit ? this_hdr->ref_n_ : trash) = this_refs;}
        {int trash; (that_unit ? that_hdr->ref_n_ : trash) = that_refs;}
        
        this->hdr_ = that_hdr;
        
        if(this_refs == 0)
          HeaderOps::template delete_header<T...>(this_hdr);
        
        return *this;
      }
      
      template<typename HeaderOps1>
      typename std::enable_if<
          std::is_base_of<HeaderOps,HeaderOps1>::value,
          future_impl_shref&
        >::type
      operator=(const future_impl_shref<HeaderOps1,T...> &that) {
        future_header *this_hdr = this->hdr_;
        future_header *that_hdr = that.hdr_;
        
        int this_refs = this_hdr->ref_n_;
        int this_unit = this_refs < 0 || this == &that ? 0 : 1;
        
        int that_refs = that_hdr->ref_n_;
        int that_unit = that_refs < 0 || this == &that ? 0 : 1;
        
        that_refs += that_unit;
        this_refs -= this_unit;
        
        {int trash; (this_unit ? this_hdr->ref_n_ : trash) = this_refs;}
        {int trash; (that_unit ? that_hdr->ref_n_ : trash) = that_refs;}
        
        this->hdr_ = that_hdr;
        
        if(this_refs == 0)
          HeaderOps::template delete_header<T...>(this_hdr);
        
        return *this;
      }
      
      future_impl_shref(future_impl_shref &&that) {
        this->hdr_ = that.hdr_;
        that.hdr_ = &future_header::the_nil;
      }
      template<typename Ops1>
      future_impl_shref(
          future_impl_shref<Ops1,T...> &&that,
          typename std::enable_if<std::is_base_of<HeaderOps,Ops1>::value>::type* = 0
        ) {
        this->hdr_ = that.hdr_;
        that.hdr_ = &future_header::the_nil;
      }
      
      future_impl_shref& operator=(future_impl_shref &&that) {
        future_header *tmp = this->hdr_;
        this->hdr_ = that.hdr_;
        that.hdr_ = tmp;
        return *this;
      }
      
      // build from some other future implementation
      template<typename Impl>
      future_impl_shref(Impl &&that) {
        this->hdr_ = that.steal_header();
      }
      
      ~future_impl_shref() {
        // unnecessary check but hopefully it helps compiler remove redundant
        // decref's when our header pointer has been moved out.
        if(this->hdr_ != &future_header::the_nil)
          HeaderOps::template decref_header<T...>(this->hdr_);
      }
    
    public:
      bool ready() const {
        return HeaderOps::is_trivially_ready_result || hdr_->status_ == future_header::status_ready;
      }
      
      upcxx::constant_function<std::tuple<T&...>> results_refs_getter() const {
        return {upcxx::tuple_refs(future_header_result<T...>::results_of(hdr_->result_))};
      }
      
      template<int i=0>
      typename std::tuple_element<i,std::tuple<T...>>::type& result() const {
        return std::get<i>(future_header_result<T...>::results_of(hdr_->result_));
      }
      
      typedef HeaderOps header_ops;
      
      future_header* steal_header() {
        future_header *hdr = this->hdr_;
        this->hdr_ = &future_header::the_nil;
        return hdr;
      }
    };
    
    
    //////////////////////////////////////////////////////////////////////
    // future_dependency: future_impl_shref specialization
    
    template<bool is_trivially_ready_result>
    struct future_dependency_shref_base;
    
    template<>
    struct future_dependency_shref_base</*is_trivially_ready_result=*/false> {
      future_header::dependency_link link_;
      
      future_dependency_shref_base(
          future_header_dependent *suc_hdr,
          future_header *arg_hdr
        ) {
        
        if(arg_hdr->status_ == future_header::status_proxying ||
           arg_hdr->status_ == future_header::status_proxying_active
          ) {
          arg_hdr = future_header::drop_for_proxied(arg_hdr);
        }
        
        if(arg_hdr->status_ != future_header::status_ready) {
          this->link_.suc = suc_hdr;
          this->link_.dep = arg_hdr;
          this->link_.sucs_next = arg_hdr->sucs_head_;
          arg_hdr->sucs_head_ = &this->link_;
          
          suc_hdr->status_ += 1;
        }
        else
          this->link_.dep = future_header::drop_for_result(arg_hdr);
      }
      
      future_dependency_shref_base(future_dependency_shref_base const&) = delete;
      future_dependency_shref_base(future_dependency_shref_base&&) = delete;
      
      void unlink_() { this->link_.unlink(); }
      
      future_header* header_() const { return link_.dep; }
    };
    
    template<>
    struct future_dependency_shref_base</*is_trivially_ready_result=*/true> {
      future_header *hdr_;
      
      future_dependency_shref_base(
          future_header_dependent *suc_hdr,
          future_header *arg_hdr
        ) {
        hdr_ = arg_hdr;
      }
      
      inline void unlink_() {}
      
      future_header* header_() const { return hdr_; }
    };
    
    template<typename HeaderOps, typename ...T>
    struct future_dependency<
        future1<future_kind_shref<HeaderOps>, T...>
      >:
      future_dependency_shref_base<HeaderOps::is_trivially_ready_result> {
      
      future_dependency(
          future_header_dependent *suc_hdr,
          future1<future_kind_shref<HeaderOps>, T...> arg
        ):
        future_dependency_shref_base<HeaderOps::is_trivially_ready_result>{
          suc_hdr,
          arg.impl_.steal_header()
        } {
      }
      
      void cleanup_early() {
        this->unlink_();
        HeaderOps::template decref_header<T...>(this->header_());
      }
      
      void cleanup_ready() {
        future_header_ops_result_ready::template decref_header<T...>(this->header_());
      }
      
      upcxx::constant_function<std::tuple<T&...>> results_refs_getter() const {
        return {upcxx::tuple_refs(future_header_result<T...>::results_of(this->header_()))};
      }
      
      future_header* cleanup_ready_get_header() {
        return this->header_();
      }
    };
  }
}
#endif
