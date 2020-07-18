# Developer's Road Map of "upcxx/src"

The intent of this document is to arm the reader with enough context to be able
to understand the source code and its comments. When this document disagrees
with the comments, please trust the latter, and thank this document for at least
making it possible for you to discern as much.

## Futures

See [src/future/README.md](future/README.md).


## Completions (src/completion.hpp)

### Completions: Events

We use incomplete types used to name completion "events". When accepting an event
as a template parameter we usually name it "Event". These event types are:

```
struct source_cx_event;
struct remote_cx_event;
struct operation_cx_event;
```


### Completions: Actions

"Actions" are values that describe how the user wants a particular event
handled. They should hold what the user gave us and little more. They encode
the event in their type, and possibly more, and then also carry whatever
runtime state is needed too. Example: `future_cx<Event,progress_level>` carries
the event and desired progress level in its type and has no runtime state since
none is required when a user calls `operation_cx::as_future()`.

```
template<typename Event, progress_level level = progress_level::user>
struct future_cx;
template<typename Event, typename ...T>
struct promise_cx;
template<typename Event>
struct buffered_cx;
template<typename Event>
struct blocking_cx;
template<typename Event, typename Fn>
struct lpc_cx;
template<typename Event, typename Fn>
struct rpc_cx;
```


### Completions: Action Lists

Actions are collected together in heterogeneous lists of type
`completions<Action...>`. When a user builds an action, we give back a
singleton list. User uses `operator|` to concatenate these into bigger lists.
Example of a singleton: `operation_cx::as_future()` returns
`completions<future_cx<operation_cx_event, progress_level::user>>`.


### Completions: Action States

When the runtime wraps on operation with completions it needs to track the user
provided actions with "state". `detail::cx_state<Action>` is specialized per
action-type to hold the necessary state (Eg for `future_cx`,
`detail::cx_state<future_cx>` holds a fresh promise).
`detail::completions_state<EventPredicate,EventValues,Cxs>` holds the
`detail::cx_state`'s for each action in `Cxs` where `Cxs = completions<...>`.
You construct it by move-passing in the user's `completions<...>&&`.

The `EventPredicate` filters out actions to carry no state and do nothing when
triggered despite what's in the `completions<...>`. This is used to partition
actions according to where they run (on this rank or not). An operation
supporting `remote_cx_event` will generally construct two `completions_state`'s
off the same `completions<...>`, one for local events, and another for remote
events, keeping the local state in some heaped memory to trigger later, and the
remote state is shipped to the target. The invocation signature to query the
predicate is `/*static constexpr bool*/ EventPredicate<Event>::value`.

The `EventValues` maps each event type to the types (as a tuple, usually empty
or singleton) of runtime values it produces (for instance `operation_cx_event`
of `rget<T>()` maps to `tuple<T>`. The invocation signature is `typename
EventValues::template tuple_t<Event>`


### Completions: Return Value Magic

`detail::completions_returner<EventPredicate,EventValues,Cxs>` has the same
type args as `detail::completions_state`, is constructed with a non-const lvalue
`detail::completions_state&`, and is used to materialize the return values
the user is expecting back. This figures out whether to return `void`, a single
future, or a tuple of futures.


### Completions: Basic Skeleton

An example of a made up operation `foo` that produces the same int `0xbeef`
for `operation_cx_event` and triggers no others.

```
// Accept all events since we aren't splitting up local vs remote events.
// Rejecting unsupported events will achieve nothing.
template<typename Event>
struct foo_event_predicate: std::true_type {};

// operation_cx produces "int", everything else empty ""
struct foo_event_values {
  template<typename Event>
  using tuple_t = typename std::conditional<
    std::is_same<Event, operation_cx_event>::value,
      std::tuple<int>,
      std::tuple<>
  >::type;
};

template<typename CxsRef>
detail::completions_returner<
    foo_event_predicate,
    foo_event_values,
    typename std::decay<CxsRef>::type
  >::return_t
foo(CxsRef &&cxs) {
  using Cxs = typename std::decay<CxsRef>::type;
  using state_t = detail::completions_state<foo_event_predice, foo_event_values, Cxs>;
  using returner_t = detail::completions_returner<foo_event_predice, foo_event_values, Cxs>;

  // Build our state on heap.
  state_t *state = new state_t(std::template forward<CxsRef>(cxs));

  // Build returner *before* injecting operation is a good policy. Doing it after
  // runs risk of operation completing synchronously and state being deleted
  // too soon (we do delete below).
  returner_t returner(*state);

  DO_FOO(
    /*invoked when op is done*/
    [=]() {
      // call state's operator(), with event as type arg and values as func args.
      state->template operator()<operation_cx_event>(0xbeef);
      // bye state
      delete state;
    }
  );

  return returner();
}
```

### Completions: Other Ways To Fire Events

Besides `detail::completions_state::operator()<Event>(T...)`, there are:

```
template<typename Event>
lpc_dormant<T...> completions_state::to_lpc_dormant(lpc_dormant<T...> *tail) &&;

template<typename Event>
SomeCallable completions_state::bind_event() &&;
```

Use `to_lpc_dormant()` to move out relevant state (matching Event) into a chain
of dormant lpc's (one per action related to Event). Round trip rpc does this to
await the remote return value and awakens the lpc from within the gex AM
handler.

Use `bind_event()` to transform this state into a callable object which will
invoke `completions_state::operator()<Event>(T...)`. Useful for `remote_cx::as_rpc`
so that we can just ship the callable via the `backend::send_am_xxx` facilities.
Currently has dangerous caveat that it **assumes** the EventPredicate has only
a single event enabled (could have multiple actions though). See comments.


## Progress

The progress engine stores nearly all of its state in the persona struct. The
meat of which is conceptually 2 lpc queues: internal and user level lpc's.
Also, at this level everything is an lpc: rpc's are deserialized as lpc's,
promises to be fulfilled also sit as lpc's. Those 2 queues become 4 queues as
we have thread safe vs unsafe versions of each. When a thread is pushing an lpc
to a persona, it dynamically checks if that persona is current with this thread
and uses the unsafe queue if possible (avoids atomic insn at cost of branch).

Routines for submitting lpc's into a persona's queue, which are members of
`detail::persona_tls` the instance of which should always be "this" thread's
singleton `detail::the_persona_tls`, are:

```
// struct detail::persona_tls {

template<bool known_active>
void enqueue(persona&, progress_level, detail::lpc_base*, std::integral_constant<bool,known_active>);

template<typename Fn, bool known_active>
void during(persona&, progress_level, Fn&&, std::integral_constant<bool,known_active>);

template<typename Fn, bool known_active>
void defer(persona&, progress_level, Fn&&, std::integral_constant<bool,known_active>);
```

`enqueue` takes a persona, level, and heaped lpc thunk and links it into the
appropriate queue of the persona. The `known_active` compile time bool
indicates whether the caller knows statically that the target persona is active
with this thread thus eliminating two queue possibilities and that dynamic
branch for persona activeness. `during` and `defer` are conveniences over
`enqueue` that build a new lpc out of the provided callable and enqueue it.
`defer` always just enqueues the callback and returns immediately. `during` is
similar excepting that if the persona is active with this thread then `during`
will attempt to execute the callable synchronously (good because avoids heap
allocation and later virtual dispatch) under the following circumstances:

  * If progress level is internal, then always.
  
  * If progress level is user, then only if we aren't currently bursting through
    a user-level queue of that persona somewhere up in the call stack. This
    prevents user callbacks from firing in the context of other user callbacks,
    a feature we assume the user is grateful for.


### Progress: About RPC's as LPC's

When an rpc is received, its non-deserialized bytes live alongside an lpc whose
`execute_and_delete` function knows how to deserialize, execute, and free up
the rpc's resources. Deferring deserialization to happen in the lpc as opposed
to immediately upon receipt is necessary to efficiently support view's and any
other type (no others exist yet) which deserialize into objects that reference
memory in the serialized buffer.


### Progress: About Promises as LPC's

`detail::promise_meta`, which lives in the promise's
`detail::future_header_promise`, inherits from `detail::lpc_base` so that it
may reside in lpc queues. The vtable fnptr member `execute_and_delete` points
to a function that applies `detail::promise_meta::deferred_decrements` to its
`countdown` and then also decrements the promise refcount. When the runtime
wants to defer fulfilling a future until a certain kind of progress is entered,
it sets the field `deferred_decrements=1` and links the promise into the queue,
and then forgets the promise ever existed. Since an lpc can be in at most one
queue at a time (intrusive linked list), the runtime needs to be sure that the
promise isn't already in a queue, or if it is, that its in the same queue we're
pushing to **and** this queue is active with this thread (otherwise there is a
race condition modifying `deferred_decrements` while the target thread could be
applying it to `countdown`).


### Progress: The Dedicated Promise Queue

Now back to the various per-persona queues, there is still one more queue to
handle the very specialized but **very common** case of a thread unsafe queue
to be fulfilled during user-level progress of promises whose value types `T...`
are all `TriviallyDestructible`. This queue can be reaped more efficiently than
the others since we know we're always fulfilling a promise thus we avoid a
virtual dispatch per lpc. And the fact that the refcount is guarding
destruction of trivially destructible types means in the case of `0 ==
--refcount` we can elide destruction (which would otherwise re-necessitate the
virtual dispatch) and just free the memory. See
`detail::promise_vtable::fulfill_deferred_and_drop_trivial()` for the lpc code
that does this.

There are two very low-level functions for submitting a promise into a persona's
progress which will leverage the special queue if possible:

  * `detail::persona_tls::fulfill_during_user_of_active`: requires static
    knowledge that the target persona is active with this thread, the
    desired progress level is user, and the promise isn't already in some other
    queue except for possibly the one we're putting it in.

  * `detail::persona_tls::enqueue_quiesced_promise`: requires static knowledge
    that the promise is "quiesced", meaning it has no other dependency counter
    work coming concurrently or later (and therefor in no other queue).

And a less low-level function `backend::fulfill_during` that the majority of
the implementation uses to submit promises, especially those **owned by the
user** (e.g. `operation_cx::as_promise`) for which properties like quiescence
are unknowable and progress level is a template parameter. It only requires the
persona be active with this thread.


## Serialization

The ways serialization can be registered for a type T:

  * Specialize such that `upcxx::is_trivially_serializable<T>::value == true`

  * Specialize `upcxx::serialization<T>`.

  * Provide member type `T::upcxx_serialization`.

  * Provide nested member type template (this has higher priority than above)
    `T::upcxx_serialization::template supply_type_please<typename>`. The
    implementation passes in the type `T` being serialized. Useful for cases
    when `T::upcxx_serialization` can't easily know this (like it was inherited
    from some base class of `T`). **Proposal**: generalize `upcxx_serialization`
    to be *possibly* templated on a type to subsume this feature, e.g. the
    implementation would first try `T::upcxx_serialization<T>::??` and if that
    dies then try `T::upcxx_serialization::??`.
    
  * Free when `std::is_trivially_copyable<T>::value == true`

The job of `serialization_traits<T>` is to determine the registration point
among the above and present it for querying through a uniform interface. The
implementation considerably extends the fidelity of serialization behavior a
type can register, which means that `serialization_traits` also calculates
default values for these extensions when they aren't specified.

The presented members of `serialization_traits<T>`, all of which are `static`,
and additionally `constexpr` for data fields, are:

  * `serialize`, `deserialize`: As indicated by the spec.
  
  * `bool is_serializable`: Whether invoking serialization is definitely
    sanctioned. Note that other members are still available even if this value
    is false. For instance, a moveable type with no serialization registered
    can still be serialized and will use the trivial mechanism to do so, but
    that hardly makes it safe. Perhaps counter intuitively, the default value
    is `true`, since that's the correct assumption when a registration point is
    discovered.

  * `bool is_actually_trivially_serializable`: Whether the underlying
    serialization mechanism is the trivial byte copy regardless of whether the
    type thinks itself TriviallySerializable. Used by other serialization
    facilities to dispatch to optimized memcpy's for bulk serialization. The
    default value is `false`.

  * `bool skip_is_fast`: Whether the sibling function `skip` exists and meets
    the criteria that it is "fast", meaning that padding the serialized
    representation with a leading byte jump delta word to implement skip would
    perform worse overall. Arguably this should be renamed to
    `skip_is_available` or even `skip_is_available_and_fast`. The default is
    `false`.
    
  * `void skip(Reader &r)`: Advance the given reader past one instance of a
    serialized `T` and do so "quickly". There is no default implementation for
    this function so it is not guaranteed to exist.

  * `SizeAfter ubound(SizeBefore prefix, T const&)`: Computes a storage
    size upper-bounding the buffer size needed to hold the serialized value as
    appended to an input buffer of size `prefix`. See the "Serialization:
    Storage Sizes" section below for valid types of `SizeAfter` and
    `SizeBefore`. If the trivial mechanism is used than this just pads `prefix`
    to `alignof(T)` and adds `sizeof(T)`. Otherwise the provided default
    returns the invalid storage size indicating no upper bound (which will
    cause `serialization_writer` to use a growable buffer).
    
  * `bool references_buffer`: Whether the deserialized object holds pointers
    into the buffer form which it was deserialized. The default value is `false`.

  * `typedef deserialized_type`: The type with pointer removed returned by
    sibling `deserialized_type* deserialize()`. It is undefined behavior if
    these do not match. The provided default calculates the unpointered return
    type of `deserialize()`.

  * `deserialized_type deserialized_value(T const&)`: Serializes to a temporary
    buffer and returns the deserialized value. This is only provided for types
    which are returnable from functions (C-style arrays are not).

  * `typedef static_ubound_t`: The storage size type as returned by
    `ubound(detail::empty_storage_size)` when it is entirely static, otherwise
    the invalid storage size type. See the "Serialization: Storage Sizes"
    section below.

  * `static_ubound_t static_ubound`: The singleton instance of `static_ubound_t`.


### Serialization: Storage Sizes

`ubound()` manipulates storage size values, which are pairs of byte sizes and
required alignments, whose types must be instantiations of
`detail::storage_size<size_t,size_t>`. This space of types and inhabiting
values creates one superspace where the values carry varying degrees of static
knowledge, with these degrees being:

  * Entirely static: Both the size and alignment values are carried in the
    type, thus permitting the runtime data to be empty.

  * Dynamic size, statically bounded alignment: This size and alignment are
    carried at runtime, but there is a static inclusive upper bound on the
    alignment.

  * Dynamic size, dynamic unbounded alignment: This size and alignment are all
    runtime. We have no static knowledge.

The primary operation over the storage size superspace is `a.cat(b)` which
returns the computed storage size needed to hold bytes for `a` followed by (in
increasing memory address order) bytes of `b`, where padding bytes are inserted
before `b` but not after. Thus storage sizes are not necessarily multiples of
their alignment. There is a designated "invalid" storage size value inhabiting
its own singleton specialization of `detail::storage_size<?,?>` (thus entirely
static) which when cat'd with any other always poisons the result to be invalid
as well. The "empty" storage size has zero bytes and requires only one byte
alignment.

Authors of `ubound` routines need not understand the complexities involved in
`detail::sotrage_size`'s implementation. The contract (or "concept" in C++
lingo) the superspace follows is conceptually simple, just remember the types
are nearly as dynamic as the runtime values, so parameterize input types and
aggressively compute return types. In the following specifications, any type
parameter beginning with the prefix "SS" is implicitly some instantiation of
`detail::storage_size<?,?>`.

The factory functions/values for storage sizes are:

  * `template<typename T> constexpr SSReturn detail::storage_size_of()`: Builds
    a completely static storage size from `sizeof(T)` & `alignof(T)`.

  * `constexpr detail::storage_size<0,1> detail::empty_storage_size`

  * `constexpr detail::invalid_storage_size_t detail::invalid_storage_size`:
    where `detail::invalid_storage_size_t` is the designated instantiation of
    `detail::storage_size<?,?>` for the singleton type having the invalid value.

Members of a storage size:

  * `SSReturn cat(SSThat that) const`: cat's `this` with `that`.

  * `template<size_t size, size_t align> SSReturn cat() const`: cat's `this`
    with a `(size,align)` storage size built from compile time values.

  * `template<typename T> cat_size_of() const`: cat's `this` with a
    `(sizeof(T),alignof(T)` storage size built from the compile time type.

  * `SSReturn cat(size_t size, size_t align) const`: cat's `this` with a
    `(size,align)` storage size built from runtime values. Always prefer a
    templated compile time variant if applicable as they return types with the
    greatest degree of static knowledge.

  * `template<typename T> SSReturn cat_ubound_of(T const &that) const`:
    Shorthand for `serialization_traits<T>::ubound(*this, that)` which is a
    huge key-stroke save when authoring ubound's.

  * `template<size_t n> SSReturn arrayed() const`: returns `this` cat'd with
    itself `n` times. Zero is an acceptable input and returns an empty storage
    size.
    
  * `SSReturn arrayed(size_t n) const`: returns `this` cat'd with itself `n`
    times. Just like its compile time sibling but obviously less preferable.

  * `constexpr size_t size_aligned(size_t min_align=1) const`: returns the size
    of this storage size padded to the next multiple of its alignment.

  * `typedef static_otherwise_invalid_t`: Equivalent to this type when it is
    entirely static (no runtime state), otherwise it is the invalid type.
    
  * `constexpr static_otherwise_invalid_t static_otherwise_invalid() const`:
    The singleton instance of `static_otherwise_invalid_t`.


### Serialization: Readers / Writers

TODO
