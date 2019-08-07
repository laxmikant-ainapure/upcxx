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
@coroutine
def required_libraries(cxt, src):
  if src == here('backend.hpp'):
    # Anyone including "backend.hpp" needs UPCXX_BACKEND defined.
    yield {'upcxx-backend': {
      'ppdefs': {
        'UPCXX_BACKEND': 1,
        'UPCXX_BACKEND_%s'%cxt.upcxx_backend_id().upper(): 1
      }
    }}

  elif src in [
      # Compiling anything that includes this requires gasnet.
      here('backend/gasnet/runtime_internal.hpp'),
      here('backend/gasnet/upc_link.h'),
      
      # We pretend that anyone including "backend/gasnet/runtime.hpp" needs
      # gasnet pp-stuff, but that isn't really the case. This is just to
      # be nice so clients can include the same gasnet headers we do.
      here('backend/gasnet/runtime.hpp')
    ]:
    yield cxt.gasnet()
  
  elif src == here('intru_queue.hpp'):
    # Anyone including "intru_queue.hpp" needs UPCXX_MPSC_QUEUE_<impl> defined.
    mpsc = cxt.env('UPCXX_MPSC_QUEUE', '', universe=['atomic','biglock'])
    
    if mpsc == '':
      # If UPCXX_MPSC_QUEUE wasn't found then look for deprecated UPCXX_LPC_INBOX
      lpc_inbox = cxt.env('UPCXX_LPC_INBOX',
                          otherwise='lockfree',
                          universe=['locked','lockfree','syncfree'])
      mpsc = {
          'lockfree': 'atomic',
          'locked': 'biglock',
          'syncfree': 'atomic'
        }[lpc_inbox]
    
    mpsc = mpsc.upper()

    # we need pthreads for biglock queue
    if mpsc == 'BIGLOCK':
      maybe_pthread = yield cxt.pthread()
    else:
      maybe_pthread = {}
    
    yield cxt.libset_merge(
      {'upcxx-mpsc-queue': {
        'ppdefs': {
          'UPCXX_MPSC_QUEUE_%s'%mpsc: 1
        }
      }},
      maybe_pthread
    )

  elif src == here('diagnostic.hpp'):
    # Anyone including "diagnostic.hpp" gets UPCXX_ASSERT_ENABLED defined.
    yield {'upcxx-diagnostic': {
      'ppdefs': {
        'UPCXX_ASSERT_ENABLED': 1 if cxt.upcxx_assert_enabled() else 0
      }
    }}
    
  elif src == here('cuda.hpp'):
    if cxt.upcxx_cuda_enabled():
      yield cxt.libset_merge(
        {'upcxx-cuda': {'ppdefs':{'UPCXX_CUDA_ENABLED':1}}},
        cxt.cuda()
      )
    else:
      yield {}
  
  else:
    # Parent "nobsrule.py" handles other cases.
    yield cxt.required_libraries(src)
