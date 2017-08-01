#include <upcxx/dist_object.hpp>

using namespace upcxx;
using namespace std;

unordered_map<digest, void*> upcxx::detail::dist_master_promises;

uint64_t upcxx::detail::dist_master_id_bump = 0;
