"""
This is a nobs rule-file. See nobs/nobs/ruletree.py for documentation
on the structure and interpretation of a rule-file.
"""

@rule()
def requires_upcxx_backend(cxt, src):
  return src in [
    here('backend/gasnet/handle_cb.cpp'),
    here('backend/gasnet/runtime.cpp'),
    here('dist_object.cpp'),
    here('upcxx.cpp')
  ]
