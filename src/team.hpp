#ifndef _461be804_4596_495a_a85c_363ba50ee08c
#define _461be804_4596_495a_a85c_363ba50ee08c

#include <upcxx/backend.hpp>
#include <upcxx/utility.hpp>

namespace upcxx {
  /* This implements just a subset of the team interface as needed by
   * `upcxx::world()` and `upcxx::local_team()`. The heavyweight deficiencies
   * are obvous: no split, collectives, etc. A lightweight deficiency that
   * *could* be implemented now, but isn't, is integration with `dist_object`
   * and rpc argument marshalling.
   */
  class team {
    // since world and local_team are contiguous, we only need to know the
    // inclusive-lower bound world rank, and exclusive-upper bound rank.
    // Obviously, world() will have lb=0 and ub=rank_n.
    intrank_t rank_lb_, rank_ub_;
    
  public:
    team(detail::internal_only, intrank_t lb, intrank_t ub):
      rank_lb_(lb),
      rank_ub_(ub) {
    }
    
    team(team const&) = delete;

    team(team&&) = delete; // We delete for now since all teams are internally owned
    
    intrank_t rank_me() const {
      return upcxx::rank_me() - rank_lb_;
    }
    
    intrank_t rank_n() const {
      return rank_ub_ - rank_lb_;
    }

    intrank_t operator[](intrank_t ix) {
      return rank_lb_ + ix;
    }
    
    intrank_t from_world(intrank_t rank) {
      return rank - rank_lb_;
    }
    intrank_t from_world(intrank_t rank, intrank_t otherwise) {
      return rank_lb_ <= rank && rank < rank_ub_
        ? rank - rank_lb_
        : otherwise;
    }
  };

  namespace detail {
    extern raw_storage<team> the_world_team;
    extern raw_storage<team> the_local_team;
  }

  inline team& world() {
    return detail::the_world_team.value;
  }
  inline team& local_team() {
    return detail::the_local_team.value;
  }

  inline bool local_team_contains(intrank_t rank) {
    return backend::rank_is_local(rank);
  }
}
#endif
