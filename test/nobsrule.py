"""
This is a nobs rule-file. See nobs/nobs/ruletree.py for documentation
on the structure and interpretation of a rule-file.
"""

# List of source files which make direct calls to gasnet, or include
# headers which do.
REQUIRES_GASNET = [
  'hello_gasnet.cpp',
]

REQUIRES_PTHREAD = [
  'hello_threads.cpp',
  'lpc_barrier.cpp',
  'rput_thread.cpp',
  'uts/uts_threads.cpp',
  'uts/uts_hybrid.cpp',
  'view.cpp',
]

REQUIRES_OPENMP = [
  'rput_omp.cpp',
  'uts/uts_omp.cpp',
  'uts/uts_omp_ranks.cpp'
]

########################################################################
### End of test registration. Changes below not recommended.         ###
########################################################################

# Converts filenames relative to this nobsfile to absolute paths.
#NO_REQUIRES_UPCXX_BACKEND = map(here, NO_REQUIRES_UPCXX_BACKEND)
REQUIRES_GASNET  = map(here, REQUIRES_GASNET)
REQUIRES_PTHREAD = map(here, REQUIRES_PTHREAD)
REQUIRES_OPENMP  = map(here, REQUIRES_OPENMP)

@rule()
def requires_gasnet(cxt, src):
  return src in REQUIRES_GASNET

@rule()
def requires_pthread(cxt, src):
  return src in REQUIRES_PTHREAD

@rule()
def requires_openmp(cxt, src):
  return src in REQUIRES_OPENMP
  
@rule()
def include_vdirs(cxt, src):
  ans = dict(cxt.include_vdirs(src)) # inherit value from parent nobsrule
  ans['upcxx-example-algo'] = here('..','example','algo')
  return ans
