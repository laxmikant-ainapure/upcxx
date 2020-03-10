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

The "seq" build of libupcxx is performance-optmized for single-threaded
processes, or for a model where only a single thread per process will ever be
invoking interprocess communication via upcxx. The performance gains with
respect to the "par" build stem from the removal of internal synchronization
(mutexes, atomic memory ops) within the upcxx runtime. Affected upcxx routines
will be observed to have lower overhead than their "par" counterparts.

The restrictions on a seq-mode client are thus:

  * Only the thread which invokes `upcxx::init()` may ever hold the master
    persona. This thread is regarded as the "primordial" thread.

  * Any inter-process communicating upcxx routine (e.g. `upcxx::rput/rget/rpc/...`)
    must be called from the primordial thread while holding the master persona.

  * Shared-heap allocation/deallocation (e.g. `upcxx::allocate/deallocate/new_/
    new_array/delete_/delete_array`) must be called from the primordial thread
    while holding the master persona.

Types of communication that do not experience restriction:

  * Intra-process communication (e.g. `upcxx::persona::lpc()`) is always OK.

  * `upcxx::progress()` is always OK. Incoming rpc's are only
    processed if progress is called from the primordial thread while it has the
    master persona.

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
upcxx::master_persona().lpc([=]() {
  upcxx::rpc(99, [=]() { std::cout << "Initiated from far away."; });
});
```
