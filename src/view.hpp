#ifndef _3493eefe_7dec_42a4_b7dc_b98f99716dfe
#define _3493eefe_7dec_42a4_b7dc_b98f99716dfe

#include <upcxx/parcel.hpp>
#include <upcxx/packing.hpp>

#include <cstdint>
#include <iterator>
#include <stdexcept>

namespace upcxx {
  //////////////////////////////////////////////////////////////////////////////
  // deserializing_iterator: Wraps a parcel_reader whose head is pointing to
  // a consecutive sequence of packed T's.
  
  template<typename T>
  class deserializing_iterator {
  public:
    using difference_type = std::ptrdiff_t;
    using value_type = T;
    using pointer = T*;
    using reference = T;
    using iterator_category = std::input_iterator_tag;
    
  private:
    parcel_reader r_;

  public:
    deserializing_iterator(char const *p = nullptr): r_(p) {}
    
    T operator*() const {
      parcel_reader r1{r_};
      raw_storage<T> raw;
      unpacking<T>::unpack(r1, &raw, std::true_type());
      return raw.value_and_destruct();
    }
    
    deserializing_iterator operator++(int) {
      deserializing_iterator old = *this;
      unpacking<T>::skip(r_);
      return old;
    }

    deserializing_iterator& operator++() {
      unpacking<T>::skip(r_);
      return *this;
    }

    friend bool operator==(deserializing_iterator a, deserializing_iterator b) {
      return a.r_.head() == b.r_.head();
    }
    friend bool operator!=(deserializing_iterator a, deserializing_iterator b) {
      return a.r_.head() != b.r_.head();
    }
    friend bool operator<(deserializing_iterator a, deserializing_iterator b) {
      return a.r_.head() < b.r_.head();
    }
    friend bool operator>(deserializing_iterator a, deserializing_iterator b) {
      return a.r_.head() > b.r_.head();
    }
    friend bool operator<=(deserializing_iterator a, deserializing_iterator b) {
      return a.r_.head() <= b.r_.head();
    }
    friend bool operator>=(deserializing_iterator a, deserializing_iterator b) {
      return a.r_.head() >= b.r_.head();
    }
  };

  //////////////////////////////////////////////////////////////////////////////
  // view_default_iterator<T>::type: Determines the best iterator type for
  // looking at a consecutive sequence of packed T's.
  
  template<typename T,
           bool definitely_trivial = is_definitely_trivially_serializable<T>::value>
  struct view_default_iterator;
  
  template<typename T>
  struct view_default_iterator<T, /*definitely_trivial=*/true> {
    using type = T*;
  };
  template<typename T>
  struct view_default_iterator<T, /*definitely_trivial=*/false> {
    using type = deserializing_iterator<T>;
  };
  
  template<typename T>
  using view_default_iterator_t = typename view_default_iterator<T>::type;

  //////////////////////////////////////////////////////////////////////////////
  // view: A non-owning range delimited by a begin and end
  // iterator which can be serialized, but when deserialized the iterator type
  // will change to its default value (deserializing_iterator_of<T>::type).
  
  namespace detail {
    template<typename T, typename Iter,
             bool trivial = packing_is_trivial<T>::value>
    struct packing_view;

    template<typename Me, typename T, typename Iter>
    struct view_pointerness {
      using iterator = Iter;
      using size_type = std::size_t;
      using difference_type = std::ptrdiff_t;
      using value_type = T;
      using pointer = T*;
      using const_pointer = T const*;
      using reference = typename std::iterator_traits<Iter>::reference;
      using const_reference = typename std::conditional<
          std::is_same<reference, T&>::value,
          T const&,
          reference
        >::type;
    };

    template<typename Me, typename T, typename T1>
    struct view_pointerness<Me, T, T1*> {
      using iterator = T const*;
      using size_type = std::size_t;
      using difference_type = std::ptrdiff_t;
      using value_type = T;
      using pointer = T const*;
      using const_pointer = T const*;
      using reference = T const&;
      using const_reference = T const&;
      using const_iterator = T const*;
      using const_reverse_iterator = std::reverse_iterator<T const*>;

      constexpr const_pointer data() const {
        return static_cast<Me const*>(this)->beg_;
      }
      
      constexpr const_iterator cbegin() const {
        return static_cast<Me const*>(this)->beg_;
      }
      constexpr const_iterator cend() const {
        return static_cast<Me const*>(this)->end_;
      }
      
      constexpr const_reverse_iterator crbegin() const {
        return const_reverse_iterator(static_cast<Me const*>(this)->end_);
      }
      constexpr const_reverse_iterator crend() const {
        return const_reverse_iterator(static_cast<Me const*>(this)->beg_);
      }
    };

    template<typename Me, typename T, typename Iter,
             typename = void>
    struct view_randomness {
      // no operator[]
    };

    template<typename Me, typename T, typename Iter>
    struct view_randomness<
        Me, T, Iter,
        typename std::conditional<true, void, decltype(std::declval<Iter>() + int())>::type
      > {

      using reference = typename view_pointerness<Me,T,Iter>::reference;
      
      reference operator[](std::size_t i) const {
        return *(static_cast<Me const*>(this)->beg_ + i);
      }

      reference at(std::size_t i) const {
        Me const *me = static_cast<Me const*>(this);
        
        if(i < me->n_)
          return *(me->beg_ + i);
        else
          throw std::out_of_range("Index out of range for view<T>.");
      }
    };

    template<typename Me, typename T, typename Iter,
             typename = void>
    struct view_backwardness {
      // No rbegin(), rend(), or back()
    };

    template<typename Me, typename T, typename Iter>
    struct view_backwardness<
        Me, T, Iter,
        typename std::conditional<true, void, decltype(--std::declval<Iter&>())>::type
      > {

      using reference = typename view_pointerness<Me,T,Iter>::reference;
      using iterator = typename view_pointerness<Me,T,Iter>::iterator;
      using reverse_iterator = std::reverse_iterator<iterator>;

      constexpr reverse_iterator rbegin() const {
        return reverse_iterator(static_cast<Me const*>(this)->end_);
      }
      constexpr reverse_iterator rend() const {
        return reverse_iterator(static_cast<Me const*>(this)->beg_);
      }

      reference back() const {
        Iter x(static_cast<Me const*>(this)->end_);
        return *--x;
      }
    };
  }
  
  template<typename T, typename Iter = view_default_iterator_t<T>>
  class view:
    public detail::view_pointerness<view<T,Iter>, T, Iter>,
    public detail::view_randomness<view<T,Iter>, T, Iter>,
    public detail::view_backwardness<view<T,Iter>, T, Iter> {
    
    friend detail::view_pointerness<view<T,Iter>, T, Iter>;
    friend detail::view_randomness<view<T,Iter>, T, Iter>;
    friend detail::view_backwardness<view<T,Iter>, T, Iter>;
    
    friend packing<view<T,Iter>>;
    friend detail::packing_view<T,Iter>;

  public:
    using iterator = typename detail::view_pointerness<view<T,Iter>, T, Iter>::iterator;
    using reference = typename detail::view_pointerness<view<T,Iter>, T, Iter>::reference;
    
  private:  
    Iter beg_, end_;
    std::size_t n_;

  public:
    constexpr view():
      beg_(), end_(), n_(0) {
    }
    constexpr view(Iter begin, Iter end, std::size_t n):
      beg_{std::move(begin)},
      end_{std::move(end)},
      n_{n} {
    }
    
    constexpr iterator begin() const { return beg_; }
    constexpr iterator end() const { return end_; }

    constexpr std::size_t size() const { return n_; }
    constexpr bool empty() const { return n_ == 0; }

    constexpr reference front() const { return *beg_; }
  };
  
  //////////////////////////////////////////////////////////////////////////////
  // make_view: Factory functions for view.
  
  template<typename Bag,
           typename T = typename Bag::value_type,
           typename Iter = typename Bag::const_iterator>
  view<T, Iter> make_view(Bag const &bag) {
    return {bag.cbegin(), bag.cend(), bag.size()};
  }

  template<typename T, std::size_t n>
  constexpr view<T, T const*> make_view(T const(&bag)[n]) {
    return {(T const*)bag, (T const*)bag + n, n};
  }

  template<typename Iter,
           typename T = typename std::iterator_traits<Iter>::value_type,
           typename = decltype(std::distance(std::declval<Iter>(), std::declval<Iter>()))>
  view<T, Iter> make_view(Iter begin, Iter end) {
    std::size_t n = std::distance(begin, end);
    return {static_cast<Iter&&>(begin), static_cast<Iter&&>(end), n};
  }
  
  template<typename Iter,
           typename T = typename std::iterator_traits<Iter>::value_type>
  constexpr view<T, Iter> make_view(Iter begin, Iter end, std::size_t n) {
    return {static_cast<Iter&&>(begin), static_cast<Iter&&>(end), n};
  }

  //////////////////////////////////////////////////////////////////////////////
  // packing<view>: specialization of packing.
  
  namespace detail {
    // Non-trivially packed T. On the wire this is two size_t's, one for the skip
    // delta and one for the sequence length and then the consecutively packed T's.
    template<typename T, typename Iter>
    struct packing_view<T, Iter, /*trivial=*/false> {
      template<typename Ub0, bool skippable>
      static auto ubound(Ub0 ub0, view<T,Iter> const &x, std::integral_constant<bool,skippable>)
        -> decltype(
          detail::packing_sequence<T>::ubound_elts(
            ub0./*delta*/template trivial_added<std::size_t>()
               ./*n    */template trivial_added<std::size_t>(),
            x.beg_, x.n_, std::true_type()
          )
        ) {
        return detail::packing_sequence<T>::ubound_elts(
          ub0./*delta*/template trivial_added<std::size_t>()
             ./*n    */template trivial_added<std::size_t>(),
          x.beg_, x.n_, std::true_type()
        );
      }
      
      template<bool skippable>
      static void pack(parcel_writer &w, view<T,Iter> const &x, std::integral_constant<bool,skippable>) {
        std::size_t *delta = w.place_trivial_aligned<std::size_t>();
        std::size_t size0 = w.size();

        std::size_t n = x.n_;
        w.put_trivial_aligned<std::size_t>(n);
        
        for(Iter i=x.beg_; n-- != 0; ++i)
          packing<T>::pack(w, *i, std::true_type());

        *delta = w.size() - size0;
      }

      using unpacked_t = view<unpacked_of_t<T>/*, default iterator*/>;
      
      static void skip(parcel_reader &r) {
        std::size_t delta = r.pop_trivial_aligned<std::size_t>();
        r.jump(delta);
      }

      template<bool skippable>
      static void unpack(parcel_reader &r, void *into, std::integral_constant<bool,skippable>) {
        std::size_t delta = r.pop_trivial_aligned<std::size_t>();
        
        parcel_reader r1{r};
        std::size_t n = r1.pop_trivial_aligned<std::size_t>();
        
        ::new(into) view<T,Iter>(
          Iter(r1.head_pointer()),
          Iter(r.head_pointer() + delta),
          n
        );

        r.jump(delta);
      }
    };

    // Trivially packed T's. On the wire this is a size_t for the element count
    // followed by the consecutively packed T's.
    template<typename T, typename Iter>
    struct packing_view<T, Iter, /*trivial=*/true> {
      template<typename Ub0, bool skippable>
      static auto ubound(Ub0 ub0, view<T,Iter> const &x, std::integral_constant<bool,skippable>) ->
        decltype(ub0.template trivial_added<std::size_t>()
                    .template trivial_array_added<T>(x.n_)) {
        return ub0.template trivial_added<std::size_t>()
                  .template trivial_array_added<T>(x.n_);
      }

      template<bool skippable>
      static void pack(parcel_writer &w, view<T,Iter> const &x, std::integral_constant<bool,skippable>) {
        std::size_t n = x.n_;
        w.put_trivial_aligned<std::size_t>(n);

        T *y = w.place_trivial_aligned<T>(n);
        for(Iter i=x.beg_; n-- != 0; ++i)
          *y++ = *i;
      }

      using unpacked_t = view<T/*, default iterator*/>;
      
      static void skip(parcel_reader &r) {
        std::size_t n = r.pop_trivial_aligned<std::size_t>();
        r.pop_trivial_aligned<T>(n);
      }

      template<bool skippable>
      static void unpack(parcel_reader &r, void *into, std::integral_constant<bool,skippable>) {
        std::size_t n = r.pop_trivial_aligned<std::size_t>();
        T *ys = const_cast<T*>(r.pop_trivial_aligned<T>(n));
        ::new(into) view<T,Iter>(ys, ys + n, n);
      }
    };
  }

  template<typename T, typename Iter>
  struct packing_screen_trivial<view<T,Iter>>:
    std::false_type {
  };
  
  template<typename T, typename Iter>
  struct packing_screened<view<T,Iter>>:
    // dispatch to detail::packing_view
    detail::packing_view<T,Iter> {

    static constexpr bool is_definitely_supported = packing_is_definitely_supported<T>::value;
    static constexpr bool is_owning = false;
  };
}

#endif
