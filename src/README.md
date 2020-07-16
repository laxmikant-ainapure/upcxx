# Developer's Road Map of "upcxx/src"

The intent of this document is to arm the reader with enough context to be able
to understand the source code and its comments. When this document disagrees
with the comments, please trust the latter, and thank this document for at least
making it possible for you to discern as much.

## Completions (src/completion.hpp)

We use incomplete types used to name completion "events". When accepting an event
as a template parameter we usually name it "Event". These event types are:

```
struct source_cx_event;
struct remote_cx_event;
struct operation_cx_event;
```

"Actions" are values that describe how the user wants a particular event
handled. They should hold what the user gave us and little more. They encode
the event in their type, and possibly more, and then also carry whatever
runtime state is needed too. Example: `future_cx<Event,progress_level>` carries
the event and desired progress level in its type and has no runtime state since
none is required when a user calls `operation_cx::as_future()`.

Action types:

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

Actions are collected together in heterogeneous lists of type
`completions<Action...>`. When a user builds an action, we give back a
singleton list. User uses `operator|` to concatenate these into bigger lists.
Example of a singleton: `operation_cx::as_future()` returns
`completions<future_cx<operation_cx_event, progress_level::user>>`.

When the runtime wraps on operation with completions it needs to track the user
provided actions with "state". `detail::cx_state<Action>` is specialized per
action-type to hold the necessary state (Eg for `future_cx`,
`detail::cx_state<future_cx>` holds a fresh promise).
`detail::completions_state<EventPredicate,EventValues,Cxs>` holds the
`detail::cx_state`'s for each action in `Cxs` where `Cxs = completions<...>`.
You construct it by move-passing in the user's `completions<...>&&`.

The `EventPredicate` filters out actions to carry no state and do nothing when
triggered despite what's in the `completions<...>`. This is uses to partition
actions according to where they run (on this rank or not). An operation
supporting `remote_cx_event` will generally construct two `completions_state`'s
off the same `completions<...>`, one for local events, and another for remote
events, keeping the local state in some heaped memory to trigger later, and the
remote state is shipped to the target. The invocation signature to query the
predicate is `/*static constexpr bool*/ EventPredicate<Event>::value`.

The `EventValues` maps each event type the types (as a tuple, usually a
singleton) of runtime values it produces (for instance `operation_cx_event` of
`rget<T>()` maps to `tuple<T>`. The invocation signature is
`typename EventValues::template tuple_t<Event>`

`detail::completions_returner<EventPredicate,EventValues,Cxs>` has the same
type args as `detail::completions_state`, is constructed with a non-const lvalue
`detail::completions_state&`, and is used to materialize the return values
the user is expecting back. This figures out whether to return `void`, a single
future, or a tuple of futures.


### You Complete Me

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
