#include <upcxx/team.hpp>

namespace detail = upcxx::detail;

using upcxx::team;
using upcxx::raw_storage;

raw_storage<team> detail::the_world_team;
raw_storage<team> detail::the_local_team;
