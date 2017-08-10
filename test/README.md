## Creating Tests ##

So far, properly creating a test only makes it compilable into an 
executable, it does not integrate with any automated testing 
frameworks. In fact, there is nothing special about tests w.r.t. any of 
the other source files in the upcxx tree except that they contain a 
`main()` and are therefor executable. The fact that the encompassing 
directory is named "test" is informative only, there are no systematic 
restrictions on the structure of this directory.

To create a new test "footest", simply create its "footest.cpp" (file
containing `main()`) in this directory. That alone is enough to make
"footest" compilable and runnable (though it won't be able to call
`upcxx::init()` until we do further registration). The test
"test/hello.cpp" is like this, it has no dependencies outside of the
C++ standard library, so once written it was ready to build and execute
via:

```bash
# setup our nobs environment, only required once
cd upcxx
. sourceme

# build & run test/hello.cpp
cd test
nobs run hello.cpp

# to build only (prints path to executable in stdout):
nobs exe hello.cpp
```

Tests like this which don't require the upcxx runtime are uncommon but 
not completely trivial. And requiring the upcxx runtime is not the same 
as requiring upcxx headers. The upcxx headers are implicitly available 
to any test (via `#include <upcxx/?.hpp>`, where "upcxx/" maps to 
"upcxx/src/" in the repo). For instance, the futures/promises 
implementation has no upcxx runtime dependencies and its test 
"test/future.cpp" pulls in the implementation headers via
`#include <upcxx/future.hpp>`. But since neither the test nor the
future/promise implementation call into the upcxx runtime, the test
was also immediately runnable after writing.

Tests leveraging the upcxx runtime will need to
`#include <upcxx/backend.hpp>`, and also register themselves in the
"test/nobsrule.py" file before they can be built. The relative path
to the test file (from the perspective of the "test/nobsrule.py" file)
must be added to the `REQUIRES_UPCXX_BAKEND` list. After that, it will
be buildable/runnable in the same manner as before.

At this time there is no master `#include <upcxx/upcxx.hpp>` header file
containing all of upcxx. Even when that exists, tests should not include
it. Instead, tests should include only the functionality-specific
headers they need so that a compilation bug in a header (common in
template code) won't poison unrelated tests.
