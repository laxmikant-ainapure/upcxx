#ifndef _4fd2caba_e406_4f0e_ab34_0e0224ec36a5
#define _4fd2caba_e406_4f0e_ab34_0e0224ec36a5

/**
 * atomic.hpp
 */

#include <atomic>
#include <upcxx/backend.hpp>
#include <upcxx/global_ptr.hpp>
#include <upcxx/rpc.hpp>

namespace upcxx {

	template <typename T>
	future<T> atomic_get(global_ptr<T> p, std::memory_order order)	{
		return rpc(p.where(), [](global_ptr<T> p) { return *p.local(); }, p);
	}

	template <typename T>
	future<> atomic_put(global_ptr<T> p, T val, std::memory_order order)	{
		return rpc(p.where(), [](global_ptr<T> p, T val) { *(p.local()) = val; }, p, val);
	}

	template <typename T>
	future<T> atomic_fetch_add(global_ptr<T> p, T val, std::memory_order order) {
		return rpc(p.where(),
							 [](global_ptr<T> p, T val) {
								 T prev_val = *p.local();
								 *p.local() += val;
								 return prev_val; },
							 p, val);
	}
	
} // namespace upcxx

#endif
