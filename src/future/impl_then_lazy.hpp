#ifndef _1f14e4c6_c238_4eb1_b9aa_5f0732405698
#define _1f14e4c6_c238_4eb1_b9aa_5f0732405698

#include <upcxx/future/core.hpp>
#include <upcxx/utility.hpp>

/* future_impl_then_lazy<FuArg, Fn, ...T>: Statically encodes that this future
 * was produced by a "then" such as "FuArg::then_lazy(Fn)" and does not
 * immediately materialize the callback dependency node but instead just holds
 * the argument and callback as separate umodified fields. The only two ways
 * the callback is actually registered with the runtime are if this impl gets
 * its header stolen via "steal_header()" or it is destructed (this is the lazy
 * aspect for which it is named).
 *
 * This gives us a sweet optimization opportunity where we can leverage the
 * following law about futures:
 *   a.then(f).then(g) === a.then([](args...) { return f(args...).then(g); });
 *
 * By holding on to the argument and callback separately we can detect such
 * chains of then's and apply this transformation. But to what benefit? Consider
 * if "a" is a regular non-trivial future that is waiting on the runtime, but
 * "f" returns a trivially ready future (like "detail::make_fast_future(...)").
 * Doing the regular a.then.then strategy materializes 2 addtional runtime
 * callbacks, but the transformed version can "see" that "f" returns a trivially
 * ready future so the "g" callback can be invoked immediately. This results in
 * only needing one runtime callback in the future graph which will invoke "f"
 * and "g" sequentially with potential for inlining!
 *
 * This impl type comes with restrictions and surprises which is why only
 * future1::then_lazy invokes it:
 *   1. The callback isn't registered until the future is destroyed or casted
 *   to the generic kind, users must take care not to hold on this future
 *   unnecesssarily.
 * 
 *   2. This impl type is not copyable. Copying it before its runtime callback
 *   is materialized would result in registering redundant runtime callbacks
 *   as the copies are destroyed, which would be especially terrible if the
 *   callback had side-effects. The fix to this is possible, we could implement a
 *   tagged-union where the impl is either the sepearated arg & callback, or a
 *   shared reference to a materialized runtime node. Copying would promote the
 *   former to the latter and then just copy the node reference. This would
 *   incur a tag branch in all of the impl's member functions, but a much worse
 *   consequence is that a sloppy user who accidentally copied this impl before
 *   chaining on another "then" or "then_lazy" would inadvertantly thwart the
 *   primary optimization transformation since the copy would obfuscate the
 *   inner callback behind a runtime node. So we make this impl move-only, thus
 *   minimizing runtime branches on a tag bit, and even better, statically
 *   enforcing that the arg and callback can be kept seperate to be fused with
 *   later chained thens which is the whole point.
 *
 * Because this impl has these restrictions, it is not a full-serivce impl and
 * therefor it breaks the implementation of sever future1 methods which effectively
 * restricts the future1 as well. Such future1's cannot be copied, they cannot
 * have their results queried, you can only chain more then's, steal the header,
 * or destroy it, all three of which take this as &&.
 */

namespace upcxx {
  //////////////////////////////////////////////////////////////////////////////
  // future_is_trivially_ready: future_impl_then_lazy specialization
  
  template<typename FuArg, typename Fn, typename ...T>
  struct future_is_trivially_ready<
      future1<detail::future_kind_then_lazy<FuArg,Fn>,T...>
    > {
    static constexpr bool value = false;
  };
  
  namespace detail {
    ////////////////////////////////////////////////////////////////////////////
    // future_then_composite_fn<Fn1,Fn2>: The callable type implementing the
    // composition of "Fn1().then(Fn2)".
    
    template<typename Fn1, typename Fn2>
    struct future_then_composite_fn;
    
    ////////////////////////////////////////////////////////////////////////////
    // future_impl_then_lazy:
    
    template<typename FuArg, typename Fn, typename ...T>
    struct future_impl_then_lazy {
      static_assert(!future_is_trivially_ready<FuArg>::value, "Can't make a future_impl_then_lazy from a trivially ready argument.");

      FuArg arg_;
      Fn fn_;
      bool must_materialize_; // are we responsible for materializing the runtime header at destruction?
    
    public:
      template<typename FuArg1, typename Fn1>
      future_impl_then_lazy(FuArg1 &&arg, Fn1 &&fn) noexcept:
        arg_(static_cast<FuArg1&&>(arg)),
        fn_(static_cast<Fn1&&>(fn)),
        must_materialize_(true) {
      }

      future_impl_then_lazy(future_impl_then_lazy const&) noexcept = delete;
      future_impl_then_lazy(future_impl_then_lazy &&that) noexcept:
        arg_(static_cast<FuArg&&>(that.arg_)),
        fn_(static_cast<Fn&&>(that.fn_)),
        must_materialize_(that.must_materialize_) {
        that.must_materialize_ = false;
      }
      
      // THESE ARE NOT IMPLEMENTED! This is intentional since we cant implement them
      // without incurring the requirement that we track (and refcount) the header
      // we materialize. We still have to provide prototypes wo that decltype
      // logic invoking these methods can see the right types.
      bool ready() const;
      std::tuple<T...> result_refs_or_vals() const&;
      std::tuple<T&&...> result_refs_or_vals() &&;
      
      using header_ops = future_header_ops_dependent;

      future_header_dependent* steal_header() && {
        UPCXX_ASSERT(must_materialize_);
        must_materialize_ = false;
        return future_then<FuArg,Fn>().make_header(static_cast<FuArg&&>(arg_), static_cast<Fn&&>(fn_));
      }

      ~future_impl_then_lazy() {
        if(must_materialize_) {
          auto *hdr = static_cast<future_impl_then_lazy&&>(*this).steal_header();
          header_ops::template dropref<T...>(hdr, /*maybe_nil=*/std::false_type());
        }
      }

      /* compuse_under(Fn2): does the transformation where our Fn is the inner
       * function, Fn2 is the outer function, and returns a future_impl_then_lazy
       * with our argument and the composed callback.
       *
       * In both compose_under_return_type and compuse_under we request the types
       * Fn2RetT which are the types found in the future returned by Fn2. This
       * is redundant since we could decltype on Fn2's invocation to get them,
       * but this is a lot of work and the layer calling us ("detail::future_then")
       * already has these so we just mandate it provides them.
       */
      template<typename Fn2, typename ...Fn2RetT>
      using compose_under_return_type = future_impl_then_lazy<FuArg, future_then_composite_fn<Fn,Fn2>, Fn2RetT...>;

      template<typename Fn2, typename ...Fn2RetT>
      future_impl_then_lazy<
          FuArg, future_then_composite_fn<Fn, typename std::decay<Fn2>::type>,
          Fn2RetT...
        >
      compose_under(Fn2 &&fn2) && {
        using Fn2d = typename std::decay<Fn2>::type;

        UPCXX_ASSERT(this->must_materialize_);
        this->must_materialize_ = false;
        
        return future_impl_then_lazy<FuArg, future_then_composite_fn<Fn,Fn2d>, Fn2RetT...>(
          static_cast<FuArg&&>(arg_),
          future_then_composite_fn<Fn,Fn2d>{
            static_cast<Fn&&>(fn_),
            static_cast<Fn2&&>(fn2)
          }
        );
      }
    };
    
    
    ////////////////////////////////////////////////////////////////////////////
    // future_dependency: future_impl_then_lazy specialization. Since we always
    // materialize the runtime header before tracking it at as a dependency we
    // we can just forward the implementation to "future_dependency<future_kind_shref>".
    
    template<typename FuArg, typename Fn, typename ...T>
    struct future_dependency<
        future1<future_kind_then_lazy<FuArg,Fn>, T...>
      >:
      future_dependency<
        future1<future_kind_shref<future_header_ops_dependent,/*unique=*/true>, T...>
      > {
      
    public:
      future_dependency(
          future_header_dependent *suc_hdr,
          future1<future_kind_then_lazy<FuArg,Fn>, T...> &&arg
        ):
        future_dependency<
          future1<future_kind_shref<future_header_ops_dependent,/*unique=*/true>, T...>
        >(suc_hdr,
          static_cast<future1<future_kind_then_lazy<FuArg,Fn>, T...>&&>(arg).impl_.steal_header()
        ) {
      }
    };

    ////////////////////////////////////////////////////////////////////////////
    // future_then_composite_fn

    template<typename Fn1, typename Fn2>
    struct future_then_composite_fn {
      Fn1 fn1;
      Fn2 fn2;

      template<typename ...Arg>
      typename future_then<
          typename detail::apply_variadic_as_future<Fn1&&, Arg&&...>::return_type,
          Fn2,/*make_lazy=*/true
        >::return_type
      operator()(Arg &&...args) && {
        using apply_fn1_as_future = detail::apply_variadic_as_future<Fn1&&, Arg&&...>;
        
        return future_then<typename apply_fn1_as_future::return_type, Fn2, /*make_lazy=*/true>()(
          apply_fn1_as_future()(static_cast<Fn1&&>(fn1), static_cast<Arg&&>(args)...),
          static_cast<Fn2&&>(fn2)
        );
      }

      template<typename ...Arg>
      typename future_then<
          typename detail::apply_variadic_as_future<Fn1 const&, Arg&&...>::return_type,
          Fn2,/*make_lazy=*/true
        >::return_type
      operator()(Arg &&...args) const& {
        using apply_fn1_as_future = detail::apply_variadic_as_future<Fn1 const&, Arg&&...>;
        
        return future_then<typename apply_fn1_as_future::return_type, Fn2, /*make_lazy=*/true>()(
          apply_fn1_as_future()(static_cast<Fn1 const&>(fn1), static_cast<Arg&&>(args)...),
          static_cast<Fn2 const&>(fn2)
        );
      }
    };
  }
}
#endif
