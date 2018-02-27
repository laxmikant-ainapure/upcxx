#ifndef _461be804_4596_495a_a85c_363ba50ee08c
#define _461be804_4596_495a_a85c_363ba50ee08c

#include <upcxx/backend.hpp>

namespace upcxx {
  namespace detail {
    struct local_team_internal_ctor {};

    /* This implements just a subset of the team interface, and it does it
     * specifically for the team returned by `upcxx::local_team()`. The
     * heavyweight deficiencies are obvous: no split, collectives, etc. A
     * lightweight deficiency that *could* be implemented now, but isn't, is
     * integration with dist_object and rpc argument marshalling.
     */
    class local_team {
    public:
      local_team(local_team_internal_ctor) {}
      local_team(local_team const&) = delete;
      local_team(local_team&&) = default;
      
      intrank_t rank_me() const {
        return upcxx::rank_me() - backend::local_peer_lb;
      }
      
      intrank_t rank_n() const {
        return backend::local_peer_ub - backend::local_peer_lb;
      }

      intrank_t operator[](intrank_t ix) {
        return backend::local_peer_lb + ix;
      }
      
      intrank_t from_world(intrank_t rank) {
        return rank - backend::local_peer_lb;
      }
      intrank_t from_world(intrank_t rank, intrank_t otherwise) {
        return backend::rank_is_local(rank)
          ? rank - backend::local_peer_lb
          : otherwise;
      }
    };

    extern local_team the_local_team;
  }

  using team = detail::local_team;
  
  inline team& local_team() {
    return detail::the_local_team;
  }

  inline bool local_team_contains(intrank_t rank) {
    return backend::local_peer_lb <= rank && rank < backend::local_peer_ub;
  }
}
#endif
