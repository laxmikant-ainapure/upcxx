template <typename T>
upcxx::future<T> fetch(upcxx::dist_object<T> &dobj, upcxx::intrank_t rank) {
   return upcxx::rpc(rank, [](upcxx::dist_object<T> &rdobj) { return *rdobj; }, dobj);
}
