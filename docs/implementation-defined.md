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
