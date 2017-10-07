"""
This is a nobs rule-file. See nobs/nobs/ruletree.py for documentation
on the structure and interpretation of a rule-file.
"""

# List of source filenames (relative to this nobsfile) which
# DON'T exercise the upcxx backend (i.e. invoke upcxx::init), or
# include headers which do.
NO_REQUIRES_UPCXX_BACKEND = [
  'future.cpp',
  'hello.cpp',
  'hello_gasnet.cpp',
  'lpc_barrier.cpp',
  'multifile.cpp',
  'multifile-buddy.cpp',
  'uts/uts_threads.cpp',
]

# List of source files which make direct calls to gasnet, or include
# headers which do.
REQUIRES_GASNET = [
  'hello_gasnet.cpp',
]

REQUIRES_PTHREAD = [
  'lpc_barrier.cpp',
  'uts/uts_threads.cpp',
  'uts/uts_hybrid.cpp',
]

########################################################################
### End of test registration. Changes below not recommended.         ###
########################################################################

# Converts filenames relative to this nobsfile to absolute paths.
NO_REQUIRES_UPCXX_BACKEND = map(here, NO_REQUIRES_UPCXX_BACKEND)
REQUIRES_GASNET  = map(here, REQUIRES_GASNET)
REQUIRES_PTHREAD = map(here, REQUIRES_PTHREAD)

# Override the rules from ../nobsrule.py to use our REQUIRES_XXX lists.
@rule()
def requires_upcxx_backend(cxt, src):
  return src not in NO_REQUIRES_UPCXX_BACKEND

@rule()
def requires_gasnet(cxt, src):
  return src in REQUIRES_GASNET

@rule()
def requires_pthread(cxt, src):
  return src in REQUIRES_PTHREAD

@rule()
def include_vdirs(cxt, src):
  ans = dict(cxt.include_vdirs(src)) # inherit value from parent nobsrule
  ans['upcxx-example-algo'] = here('..','example','algo')
  return ans
