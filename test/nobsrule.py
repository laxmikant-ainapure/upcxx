"""
This is a nobs rule-file. See nobs/nobs/ruletree.py for documentation
on the structure and interpretation of a rule-file.
"""

# List of source filenames (relative to this nobsfile) which
# exercise the upcxx backend (i.e. invoke upcxx::init), or include
# headers which do.
REQUIRES_UPCXX_BACKEND = [
  'dist_object.cpp',
  'hello_upcxx.cpp',
  'multifile.cpp',
  'multifile-buddy.cpp',
  'rpc_barrier.cpp',
  'rpc_ff_ring.cpp',
  'rput.cpp',
  'atomics.cpp',
  'collectives.cpp',
]

# List of source files which make direct calls to gasnet, or include
# headers which do.
REQUIRES_GASNET = [
  'hello_gasnet.cpp',
]

########################################################################
### End of test registration. Changes below not recommended.         ###
########################################################################

# Converts filenames relative to this nobsfile to absolute paths.
REQUIRES_UPCXX_BACKEND = map(here, REQUIRES_UPCXX_BACKEND)
REQUIRES_GASNET        = map(here, REQUIRES_GASNET)

# Override the rules from ../nobsrule.py to use our REQUIRES_XXX lists.
@rule()
def requires_upcxx_backend(cxt, src):
  return src in REQUIRES_UPCXX_BACKEND

@rule()
def requires_gasnet(cxt, src):
  return src in REQUIRES_GASNET

@rule_memoized()
class include_paths_tree:
  """
  Setup a shim directory containing two symlinks named 'upcxx' and
  'upcxx-example-algo' which point to 'upcxx/src' and
  'upcxx/example/algo' respectively. With this directory added via
  '-I...' to compiler flags, allows those headers to be accessed via:
    #include <upcxx/*.hpp>
    #include <upcxx-example-algo/*.hpp>
  """
  def execute(cxt):
    return cxt.mktree({
        'upcxx': here('..','src'),
        'upcxx-example-algo': here('..','example','algo')
      },
      symlinks=True
    )
