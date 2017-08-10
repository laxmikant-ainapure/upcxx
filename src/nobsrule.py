"""
This is a nobs rule-file. See nobs/nobs/ruletree.py for documentation
on the structure and interpretation of a rule-file.
"""

@rule()
def requires_upcxx_backend(cxt, src):
  return src in [
    here('backend/gasnet1_seq/backend.cpp'),
    here('dist_object.cpp'),
  ]
