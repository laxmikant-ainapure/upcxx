#ifndef _ba798e6d_cac8_4c55_839e_7b4ba217212c
#define _ba798e6d_cac8_4c55_839e_7b4ba217212c

#include <upcxx/bind.hpp>
#include <upcxx/backend_fwd.hpp>
#include <upcxx/digest.hpp>
#include <upcxx/utility.hpp>

#include <unordered_map>

/* This is the forward declaration(s) of upcxx::team and friends. It does not
 * define the function bodies nor does it pull in the full backend header.
 */

////////////////////////////////////////////////////////////////////////////////

namespace upcxx {
  namespace detail {
    extern std::unordered_map<digest, void*> registry;
    
    // Get the promise pointer from the master map.
    template<typename T>
    promise<T>* registered_promise(digest id, int initial_anon=0);
  }
  
  class team;
  
  struct team_id {
  //private:
    digest dig_;
    
  //public:
    team& here() const {
      return *static_cast<team*>(detail::registry[dig_]);
    }
    
    #define UPCXX_COMPARATOR(op) \
      friend bool operator op(team_id a, team_id b) {\
        return a.dig_ op b.dig_; \
      }
    UPCXX_COMPARATOR(==)
    UPCXX_COMPARATOR(!=)
    UPCXX_COMPARATOR(<)
    UPCXX_COMPARATOR(<=)
    UPCXX_COMPARATOR(>)
    UPCXX_COMPARATOR(>=)
    #undef UPCXX_COMPARATOR
  };
  
  inline std::ostream& operator<<(std::ostream &o, team_id x) {
    return o << x.dig_;
  }
}

namespace std {
  template<>
  struct hash<upcxx::team_id> {
    size_t operator()(upcxx::team_id id) const {
      return hash<upcxx::digest>()(id.dig_);
    }
  };
}

namespace upcxx {
  class team:
      backend::team_base /* defined by <backend>/runtime_fwd.hpp */ {
    digest id_;
    std::uint64_t coll_counter_;
    intrank_t n_, me_;
    
  public:
    team(detail::internal_only, backend::team_base &&base, digest id, intrank_t n, intrank_t me);
    team(team const&) = delete;
    team(team &&that);
    ~team();
    
    intrank_t rank_n() const { return n_; }
    intrank_t rank_me() const { return me_; }
    
    intrank_t from_world(intrank_t rank) {
      return backend::team_rank_from_world(*this, rank);
    }
    intrank_t from_world(intrank_t rank, intrank_t otherwise) {
      return backend::team_rank_from_world(*this, rank, otherwise);
    }
    
    intrank_t operator[](intrank_t peer) const {
      return backend::team_rank_to_world(const_cast<team&>(*this), peer);
    }
    
    team_id id() const {
      return team_id{id_};
    }
    
    static constexpr intrank_t color_none = -0xbad;
    
    team split(intrank_t color, intrank_t key);
    
    void destroy(entry_barrier eb = entry_barrier::user);
    
    ////////////////////////////////////////////////////////////////////////////
    // internal only
    
    team_base& base(detail::internal_only) {
      return *this;
    }
    
    digest next_collective_id(detail::internal_only) {
      return id_.eat(coll_counter_++);
    }
  };
  
  team& world();
  team& local_team();
  
  namespace detail {
    extern raw_storage<team> the_world_team;
    extern raw_storage<team> the_local_team;    
  }
}
#endif
