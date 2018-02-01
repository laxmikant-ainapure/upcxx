"""
This is a nobs rule-file. See nobs/nobs/ruletree.py for documentation
on the structure and interpretation of a rule-file.
"""

@rule()
def requires_pthread(cxt, src):
  return src in [
    here('lpc/inbox_locked.hpp')
  ]

@rule()
def required_libraries(cxt, src):
  if src == here('backend.hpp'):
    # Anyone including "backend.hpp" needs UPCXX_BACKEND defined.
    return {'upcxx-backend': {
      'ppdefs': {
        'UPCXX_BACKEND': 1,
        'UPCXX_BACKEND_%s'%cxt.upcxx_backend_id().upper(): 1
      }
    }}

  elif src in [
      # Compiling anything that includes this requires gasnet.
      here('backend/gasnet/runtime_internal.hpp'),
      
      # We pretend that anyone including "backend/gasnet/runtime.hpp" needs
      # gasnet pp-stuff, but that isn't really the case. This is just to
      # be nice so clients can include the same gasnet headers we do.
      here('backend/gasnet/runtime.hpp')
    ]:
    return cxt.gasnet()
  
  elif src == here('lpc/inbox.hpp'):
    # Anyone including "lpc_inbox.hpp" needs UPCXX_LPC_INBOX_<impl> defined.
    return {'upcxx-lpc-inbox': {
      'ppdefs': {
        'UPCXX_LPC_INBOX_%s'%cxt.upcxx_lpc_inbox_id(): 1
      }
    }}

  elif src == here('diagnostic.hpp'):
    # Anyone including "diagnostic.hpp" gets UPCXX_ASSERT_ENABLED defined.
    return {'upcxx-diagnostic': {
      'ppdefs': {
        'UPCXX_ASSERT_ENABLED': 1 if cxt.upcxx_assert_enabled() else 0
      }
    }}
    
  else:
    # Parent "nobsrule.py" handles other cases.
    return cxt.required_libraries(src)
