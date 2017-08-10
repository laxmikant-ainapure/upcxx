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
  'rpc_barrier.cpp',
  'rpc_ff_ring.cpp',
  'rput.cpp',
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
def requires_libgasnet(cxt, src):
  return src in REQUIRES_GASNET
