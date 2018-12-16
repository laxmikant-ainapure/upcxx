# UPC++ Interoperability with Berkeley UPC #

UPC++ now has experimental support for interoperability with the 
[Berkeley UPC Runtime](http://upc.lbl.gov) (a.k.a "UPCR"), 
using any of the four UPC translators targetting that runtime.
This makes it possible to run hybrid applications that use both UPC and UPC++
(in separate object files, due to the difference in base languages).

## UPC++ / Berkeley UPC Runtime Hybrid Usage Basics

The UPC and UPC++ layers can be initialized in either order - `upcxx::init()` 
will detect if UPC has been linked in and initialize UPC if necessary.

Both layers may be active simultaneously, and shared objects from either layer are also 
valid shared objects in the other layer - however there are some important caveats. 
In particular, the `upcxx::global_pointer` and UPC pointer-to-shared
representations are NOT interchangable. Passing of shared objects across layers should be
accomplished by "down-casting" to a raw C pointer (ie `void *`) on a process with affinity
to the shared object (eg in UPC this is done using a `(void*)` cast, in UPC++ use `global_pointer<T>::local()`).
The raw pointer can then be passed across layers, and "up-cast" using the
appropriate function (i.e. `upcxx::try_global_pointer()` or `bupc_inverse_cast()`).
See the documentation for each model for details on up-casting/down-casting.

## UPC++ / Berkeley UPC Runtime Hybrid Build Rules

* UPC++ must be version 2018.9.5 or newer (`UPCXX_VERSION=20180905`)
* UPCR must be version 2018.5.3 or newer 
  (visible via `__BERKELEY_UPC{,_MINOR,PATCHLEVEL}__` or `UPCR_RUNTIME_SPEC_{MAJOR,MINOR}=3,13`)
* Both packages must be configured with the same release version of GASNet-EX,
  and compatible settings for any non-default GASNet configure options.
* The C++ compiler used for UPC++ must be ABI compatible with the backend C compiler configured for UPCR.
* UPCR must use `upcc -nopthreads` mode (ie UPC++ interop does not support pthread-as-UPC-thread mode).
* All objects linked into one executable must agree upon GASNet conduit, debug mode and thread-safety setting.
* If `UPCXX_THREADMODE=par`, then must pass `upcc -uses-threads`.
  This in turn may require UPCR's `configure --enable-uses-threads`.
* The link command should use the UPCR link wrapper, and specify `upcc -link-with='upcxx <args>'`.
* If the `main()` function appears outside UPC code, the link command should include `upcc -extern-main`.

[test/interop/Makefile](test/interop/Makefile) provides examples of this process in action.

## Running UPC++ / Berkeley UPC Runtime Hybrid programs

Resulting executables can be run using either upcrun or upcxx-run (or in many cases, 
the normal system mpirun equivalent), the job layout options are very similar.
However for obvious reasons, the model-specific scripts only have command-line options for altering 
model-specific behaviors of their own model (implemented by setting environment variables). 
If one needs non-default runtime behaviors from both models, then the recommended mechanism is to 
manually set the appropriate environment variables. Both upcrun and upcxx-run scripts have `-v` 
options that output the environment variables set to effect a given set of command-line options.

Note that special care must usually be given to the shared heap settings.

For the default `UPCXX_USE_UPC_ALLOC=yes` mode: (recommended)

  In this mode, UPC++ uses the UPCR non-collective shared heap allocator directly to service all 
  UPC++ shared allocations. In this mode, UPC++ shared heap controls are disabled and the size of the
  shared heap (shared by both models) is controlled by Berkeley UPC Runtime.
  See documentation for `upcc -shared-heap` and `UPC_SHARED_HEAP_SIZE` for details on controlling size.
  Note that UPC++ shared heap allocation failures (ie out of memory) are fatal in this mode.

For `UPCXX_USE_UPC_ALLOC=no` mode:

  In this mode, the UPC++ shared heap is created inside `upcxx::init()` by allocating one large block
  from the Berkeley UPC Runtime allocator. By default this block is allocated from the UPCR
  non-collective shared heap, but `UPCXX_UPC_HEAP_COLL=yes` changes this to use the UPCR collective shared heap.
  In both cases, there must be sufficient (non-fragmented) free space in the selected UPCR heap to
  accommodate the UPC++ shared heap during the call to `upcxx::init()`.
  In this mode, the UPC and UPC++ shared heap sizes are controlled independently by the appropriate
  spawner args or envvar settings - the UPC shared heap size must be set large enough to allow space
  for the UPC++ shared heap block creation. Note that UPCR reserves guard pages at either end of the 
  UPC shared heap and statically-allocated shared UPC objects also consume space in the UPC shared heap,
  so one should generally allow some padding in addition to anticipated shared heap consumption from 
  dynamically allocated UPC shared objects.
  For more details, see [UPCR memory management](http://upc.lbl.gov/docs/system/runtime_notes/memory_mgmt.shtml)

