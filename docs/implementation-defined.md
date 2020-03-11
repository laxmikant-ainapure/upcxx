# UPC++ Implementation Defined Behavior #

This document describes stable features supported by this implementation that
go beyond the requirements of the UPC++ Specification.

## Version Identification ##

The following macro definitions are provided by `upcxx/upcxx.hpp`:

  * `UPCXX_VERSION`:
    An integer literal providing the release version of the implementation, 
    in the format [YYYY][MM][PP] corresponding to release YYYY.MM.PP
  * `UPCXX_SPEC_VERSION`:
    An integer literal providing the revision of the UPC++ specification
    to which this implementation adheres. See the specification for the specified value.

## Serialization ##

The following `std::` types enjoy the following serialization behavior:

  * pair<A,B>:
    + DefinitelyTriviallySerializable: so long as A and B are DefinitelyTriviallySerializable.
    + DefinitelySerializable: so long as A and B are DefinitelySerializable.
    
  * tuple<T...>:
    + DefinitelyTriviallySerializable: so long as each T is DefinitelyTriviallySerializable.
    + DefinitelySerializable: so long as each T is DefinitelySerializable.
  
  * array<T,n>:
    + DefinitelyTriviallySerializable: so long as T is DefinitelyTriviallySerializable.
    + DefinitelySerializable: so long as T is DefinitelySerializable.
  
  * vector<T,Allocator>, deque<T,Allocator>, list<T,Allocator>:
    + DefinitelySerializable: so long as T is DefinitelySerializable.
  
  * set<Key,Compare,Allocator>, multiset<Key,Compare,Allocator>:
    + DefinitelySerializable: so long as Key is DefinitelySerializable.
  
  * unordered_set<Key,Hash,KeyEqual,Allocator>, unordered_multiset<Key,Hash,KeyEqual,Allocator>:
    + DefinitelySerializable: so long as Key is DefinitelySerializable.
  
  * map<Key,T,Compare,Allocator>, multimap<Key,T,Compare,Allocator>:
    + DefinitelySerializable: so long as Key and T are DefinitelySerializable.

  * unordered_map<Key,T,Compare,Allocator>, unordered_multimap<Key,T,Compare,Allocator>:
    + DefinitelySerializable: so long as Key and T are DefinitelySerializable.


## UPCXX_THREADMODE=seq Restrictions ##

The "seq" build of libupcxx is performance-optimized for single-threaded
processes, or for a model where only a single thread per process will ever be
invoking interprocess communication via upcxx. The performance gains with
respect to the "par" build stem from the removal of internal synchronization
(mutexes, atomic memory ops) within the upcxx runtime. Affected upcxx routines
will be observed to have lower overhead than their "par" counterparts.

Whereas "par-mode" libupcxx permits the full generality of the UPC++
specification with respect to multi-threading concerns, "seq" imposes these
additional restrictions on the client application:

  * Only the thread which invokes `upcxx::init()` may ever hold the master
    persona. This thread is regarded as the "primordial" thread.

  * Any upcxx routine with internal or user-progress (typically inter-process
    communication, e.g. `upcxx::rput/rget/rpc/...`) must be called from the
    primordial thread while holding the master persona. There are some routines
    which are excepted from this restriction and are listed below.

  * Shared-heap allocation/deallocation (e.g. `upcxx::allocate/deallocate/new_/
    new_array/delete_/delete_array`) must be called from the primordial thread
    while holding the master persona.

Note that these restrictions must be respected by all object files linked into
the final executable, as they are all sharing the same libupcxx.

Types of communication that do not experience restriction:

  * Sending lpc's via `upcxx::persona::lpc()` or `<completion>_cx::as_lpc()`
    has no added restriction.

  * `upcxx::progress()` and `upcxx::future::wait()` have no added restriction.
    Incoming rpc's are only processed if progress is called from the primordial
    thread while it has the master persona.

  * Upcasting/downcasting shared heap memory (e.g. `global_ptr::local()`) is
    always OK. This facilitates a kind of interprocess communication via native
    CPU shared memory access which is permitted in "seq". Note that
    `upcxx::rput/rget` is still invalid from non-primordial threads even when
    the remote memory is downcastable locally.

The legality of lpc and progress from the non-primordial thread permits users
to orchestrate their own "funneling" strategy, e.g.:

```
#!c++

// How a non-primordial thread can tell the master persona to put an rpc on the
// wire on its behalf.
upcxx::master_persona().lpc_ff([=]() {
  upcxx::rpc_ff(99, [=]() { std::cout << "Initiated from far away."; });
});
```
