@rule()
@coroutine
def required_libraries(cxt, src):
  if src == here('common/operator_new.hpp'):
    # read user's allocator choice from env
    OPNEW = cxt.env('opnew', 'std', universe=['std','ltalloc','insane'])
    OPNEW_INLINE = cxt.env('opnew_inline', 0, universe=[0,1])
    
    if OPNEW == 'ltalloc':
      # Need to download ltalloc files since we're being defensive about not
      # shipping code which contains licenses involving the term "Copyright".
      import os
      for name in ['ltalloc.cc', 'ltalloc.h']:
        if not os.path.exists(here('common/'+name)):
          yield cxt.download(
            'https://raw.githubusercontent.com/r-lyeh-archived/ltalloc/master/'+name,
            to=here('common/'+name)
          )
    
    # convert to macro number
    OPNEW = {'std':0, 'ltalloc':1, 'insane':2}[OPNEW]
    
    yield {'operator_new': {
      'ppdefs': dict(OPNEW=OPNEW, OPNEW_INLINE=OPNEW_INLINE)
    }}
  
  elif src == here('put_flood.cpp'):
    yield {'put_flood': {
      'ppdefs': dict(PROGRESS_PERIOD = cxt.env('progress_period', None))
    }}
    
  else:
    yield cxt.required_libraries(src)

