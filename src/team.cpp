#include <upcxx/team.hpp>

namespace detail = upcxx::detail;

detail::local_team detail::the_local_team{detail::local_team_internal_ctor()};
