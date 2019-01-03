"""
nobs.async: Asynchronous programming constructs based on futures and
coroutines.
"""
def _everything():
  import __builtin__
  import collections
  import os
  import sys
  import traceback
  import types

  from . import errorlog
  
  def export(obj):
    globals()[obj.__name__] = obj
    return obj
  
  AssertionError = __builtin__.AssertionError
  BaseException = __builtin__.BaseException
  dict = __builtin__.dict
  Exception = __builtin__.Exception
  getattr = __builtin__.getattr
  isinstance = __builtin__.isinstance
  iter = __builtin__.iter
  KeyError = __builtin__.KeyError
  KeyboardInterrupt = __builtin__.KeyboardInterrupt
  len = __builtin__.len
  list = __builtin__.list
  map = __builtin__.map
  object = __builtin__.object
  set = __builtin__.set
  super = __builtin__.super
  
  deque = collections.deque
  
  types_GeneratorType = types.GeneratorType

  @export
  class Job(object):
    def join(me): pass
    def cancel(me): pass

  jobs = deque()
  
  concurrency_limit = os.environ.get("UPCXX_NOBS_THREADS","")
  try: concurrency_limit = int(concurrency_limit)
  except: concurrency_limit = 0
  
  if concurrency_limit <= 0:
    import multiprocessing
    concurrency_limit = multiprocessing.cpu_count()

  @export
  def launched(job_fn):
    def proxy(*args, **kws):
      ans = Promise()
      if len(jobs) < concurrency_limit:
        try:
          jobs.append((job_fn(*args, **kws), ans))
        except (AssertionError, KeyboardInterrupt):
          raise
        except BaseException as e:
          ans.satisfy(Failure(e))
      else:
        jobs.append(((job_fn, args, kws), ans))
      return ans
    proxy.__wrapped__ = job_fn
    proxy.__name__ = job_fn.__name__
    return proxy

  @errorlog.at_shutdown
  def shutdown():
    tmp = list(jobs)[0:concurrency_limit]
    while tmp:
      try:
        job, ans = tmp.pop()
        if not isinstance(job, tuple):
          job.cancel()
      except: pass
  
  actives = deque()
  actives_append = actives.append
  actives_popleft = actives.popleft

  progressing = [0]
  def progress():
    if progressing[0]: return
    progressing[0] += 1

    try:
      while actives:
        actives_popleft()._fire()
    finally:
      progressing[0] -= 1

  def wait(until):
    while True:
      progressing[0] += 1
      try:
        while actives:
          actives_popleft()._fire()
      finally:
        progressing[0] -= 1
      
      if until._status == -1:
        return until._result
      
      if jobs:
        sats = []
        job, ans = jobs.popleft()
        try:
          sats.append((ans, Result(job.join())))
        except (AssertionError, KeyboardInterrupt):
          raise
        except BaseException as e:
          sats.append((ans, Failure(e)))
        
        while len(jobs) >= concurrency_limit:
          (job_fn, args, kws), ans = jobs[concurrency_limit-1]
          try:
            job = job_fn(*args, **kws)
            jobs[concurrency_limit-1] = (job, ans)
            break
          except (AssertionError, KeyboardInterrupt):
            raise
          except BaseException as e:
            del jobs[concurrency_limit-1]
            sats.append((ans, Failure(e)))

        for fu,res in sats:
          fu.satisfy(res)
      else:
        raise AssertionError("wait() on unsatisfiable future.")
  
  def enter_done(fu, result):
    fu._result = result._result
    fu._status = -1 # release-store
    
    for suc in fu._sucs:
      suc._status -= 1
      if suc._status == 0:
        actives_append(suc)

    progress()
  
  def fresh(fu):
    if fu._status == 0:
      actives_append(fu)
      progress()
    return fu

  def add_successor(dep, suc):
    if dep._status == -1:
      return 0
    dep._sucs.append(suc)
    return 1

  def satisfy(fu):
    fu._status -= 1
    if fu._status == 0:
      actives_append(fu)
      progress()
  
  @export
  class Future(object):
    """
    The base class for all future-like types. Futures represent an
    eventually available collection of values with a structure
    matching the arguments needed for calling general python
    functions: a positional argument list and a keywords argument list.
    The common case of a future representing a single value will have a
    positional list of length one containing that value, and no keywords
    arguments. Futures may also represent a single thrown exception
    value.
    """
    __slots__ = ('_status','_result','_sucs')
    
    def __rshift__(arg, lam):
      """
      Schedule `lam` to execute with the values provided by this future
      as its arguments. The return of `lam` will wrapped in a future
      if it isn't already a future. The return of this function will be
      a proxy for that future eventually produced by `lam`. If this future
      represnet an exceptional return, then `lam` will not be called and
      the resulting future of this operation will also have the same
      exceptional value.
      """
      return fresh(Mbind(arg, lam, jailed=False))
    
    def success(me):
      """
      Determine if this future resulted in a successful return value.
      This future must be in its ready state.
      """
      return isinstance(me._result, Result)
    
    def result(me):
      """
      Retrieve the final future instance represented by this future.
      The final future will be an instance of either the Result or
      Failure future subclasses. This future must be in its ready state.
      """
      return me._result
    
    def value(me):
      """
      Return first positional value of this future. If this future has
      no positional values, `None` will be returned. This future must be
      ready
      """
      return me._result.value()
    
    def values(me):
      """
      Return the tuple of positional values. This future must be ready.
      """
      return me._result.values()
    
    def kws(me):
      """
      Return the dictionary of keywords values. This future must be ready.
      """
      return me._result.kws()
    
    def explode(me):
      """
      If this future represents an exceptional value, raises that
      exception. Otherwise no-op. This future must be ready.
      """
      return me._result.explode()
    
    def __getitem__(me, i):
      """
      If `i` is a string then returns that keyword value of the future.
      If `i` is an integer then returns that positional value of the future.
      This future must be ready.
      """
      return me._result[i]
    
    def wait(me):
      """
      Cause this thread to make progress until this future is ready.
      Returns `value()` of this future.
      """
      return wait(me).value()
    
    def wait_futurized(me):
      """
      Cause this thread to make progress until this future is ready.
      Returns `result()` of this future.
      """
      return wait(me)
  
  @export
  class Result(Future):
    """
    Future subclass representing a final-state positional value list
    and a keywords value dictionary.
    """
    __slots__ = Future.__slots__ + ('_val_seq','_val_kws')
    
    def __init__(me, *values_seq, **values_kws):
      """
      Construct this future to have the same positional and keywords
      values that this function was called with.
      """
      me._status = -1
      me._result = me
      me._sucs = None
      me._val_seq = values_seq
      me._val_kws = values_kws
    
    def value(me):
      seq = me._val_seq
      kws = me._val_kws
      if len(kws) == 0:
        if len(seq) == 0:
          return None
        if len(seq) == 1:
          return seq[0]
      return me
    
    def values(me):
      return me._val_seq

    def kws(me):
      return me._val_kws
    
    def __getitem__(me, i):
      if isinstance(i, basestring):
        return me._val_kws[i]
      else:
        return me._val_seq[i]
    
    def __getstate__(me):
      return (me._val_seq, me._val_kws)

    def __setstate__(me, s):
      me.__init__(*s[0], **s[1])

  @export
  class Failure(Future):
    """
    Future subclass representing a final state exceptional value raised
    from a given traceback.
    """
    __slots__ = Future.__slots__ + ('exception','traceback')
    
    def __init__(me, exception, traceback=None):
      """
      Construct this future to represent the given exception value and
      traceback. If `traceback` is absent or None, then it will be
      initialized to the current traceback in `sys.exc_info()`.
      """
      me._status = -1
      me._result = me
      me._sucs = None
      me.exception = exception
      me.traceback = traceback if traceback is not None else sys.exc_info()[2]
    
    def explode(me):
      raise me.exception, None, me.traceback
    
    value = explode
    values = explode
    kws = explode
    
    def __getitem__(me, i):
      me.explode()
    
    def __getstate__(me):
      return me.exception
    
    def __setstate__(me, s):
      me.__init__(exception=s, traceback=None)

  @export
  class Promise(Future):
    """
    A promise is a user-modifiable single-assignment future which is
    explicitly satisifed to represent another future or given value.
    """
    __slots__ = Future.__slots__ + ('_arg',)
    
    def __init__(me):
      super(Promise, me).__init__()
      me._status = 1
      me._sucs = []
      
    def _fire(me):
      arg = me._arg._result
      me._arg = None
      enter_done(me, arg)
    
    def satisfy(me, *args, **kwargs):
      """
      Set this promise to represent the future obtained from 
      `futurize(*args, **kwargs)`.
      """
      arg = futurize(*args, **kwargs)
      me._arg = arg
      me._status += add_successor(arg, me)
      satisfy(me)
  
  @export
  class Mbind(Future):
    __slots__ = Future.__slots__ + ('_fire',)
    
    def __init__(me, arg, lam, jailed):
      super(Mbind, me).__init__()
      
      me._sucs = []
      me._status = add_successor(arg, me)
      
      def fire1():
        arg_result = arg._result
        try:
          if jailed:
            proxied = lam(arg_result)
          else:
            if isinstance(arg_result, Result):
              proxied = lam(*arg_result._val_seq, **arg_result._val_kws)
            elif isinstance(arg_result, Failure):
              proxied = arg_result
            else:
              assert False
          proxied = futurize(proxied)
        except (AssertionError, KeyboardInterrupt):
          raise
        except BaseException as e:
          proxied = Failure(e)
        
        # enter proxying state
        if proxied._status in (-2,-3): # acquire load
          proxied = proxied._proxied
        
        if 0 == add_successor(proxied, me):
          enter_done(me, proxied)
        else:
          me._fire = make_fire2(proxied)
          me._status = -2 # release store
      
      def make_fire2(proxied):
        def fire():
          assert me._status == -3
          me._fire = None
          enter_done(me, proxied._result)
        return fire
      
      # register method
      me._fire = fire1
  
  @export
  class All(Future):
    def __init__(me, *args, **kws):
      super(All, me).__init__()
      me._sucs = []
      
      status = 0
      for arg in args:
        status += add_successor(arg, me)
      for k in kws:
        status += add_successor(kws[k], me)
      me._status = status
    
      def _fire():
        ans_seq = ()
        ans_kws = {}
        result = None
        
        for a in args:
          a = a._result
          if isinstance(a, Result):
            ans_seq += a.values()
            ans_kws.update(a.kws())
          elif isinstance(a, Failure):
            result = a
            break
          else:
            assert False
        
        if result is None:
          for k in kws:
            f = kws[k]._result
            if isinstance(f, Result):
              assert len(f._val_seq) in (0,1) and len(f._val_kws) == 0
              ans_kws[k] = f._val_seq[0] if len(f._val_seq) == 1 else None
            elif isinstance(f, Failure):
              result = f
              break
            else:
              assert False
        
        if result is None:
          result = Result(*ans_seq, **ans_kws)
        
        enter_done(me, result)
      # register method
      me._fire = _fire
  
  @export
  class WhenDone(Future):
    def __init__(me, args):
      super(WhenDone, me).__init__()
      me._sucs = []
      
      status = 0
      for arg in args:
        status += add_successor(arg, me)
      me._status = status
    
    def _fire(me):
      enter_done(me, Result())
  
  @export
  class WhenSucceeded(Future):
    __slots__ = Future.__slots__ + ('_args',)
    
    def __init__(me, args):
      super(WhenSucceeded, me).__init__()
      me._sucs = []
      me._args = args
      
      status = 0
      for arg in args:
        status += add_successor(arg, me)
      me._status = status
    
    def _fire(me):
      args = me._args
      ans = Result()
      for arg in args:
        if isinstance(arg._result, Failure):
          ans = arg._result
          break
      enter_done(me, ans)
  
  @export
  class Coroutine(Future):
    __slots__ = Future.__slots__ + ('_fire','__name__')
    
    def __init__(me, gen):
      super(Coroutine, me).__init__()
      me._status = 0
      me._sucs = []
      
      me.__name__ = gen.__name__
      
      gen_send = gen.send
      gen_throw = gen.throw
      
      def make_fire(arg0):
        def fire():
          arg = arg0._result
          
          while True:
            assert isinstance(arg, (Result, Failure))
            try:
              if isinstance(arg, Result):
                arg = gen_send(arg.value())
              elif isinstance(arg, Failure):
                arg = gen_throw(arg.exception, None, arg.traceback)
              else:
                assert False
              stopped = False
            except StopIteration:
              stopped = True
            except (AssertionError, KeyboardInterrupt):
              raise
            except BaseException as e:
              arg = Failure(e)
              stopped = True
            
            if stopped:
              me._fire = None
              enter_done(me, arg)
              break
            else:
              arg = futurize(arg)
              status = add_successor(arg, me)
              if status != 0:
                me._fire = make_fire(arg)
                me._status = status
                break
              else:
                arg = arg._result
        
        return fire
      
      # register method
      me._fire = make_fire(Result(None))

  @export
  def futurize(*args, **kws):
    """
    Wrap and collect the given function arguments into a single returned
    future. If all of `args` and `kws` are non-futures then a future
    representing those positional and keywords values will be returned.
    Otherwise, the future returned will have its positional values
    represent the concatenated positional values from the
    `map(futurize, args)` list of futures, and the returned future's 
    keywords arugments will be a best-effort attempt to merge the keywords
    values from each future in `args` along with each of the non-futures
    or singly-valued futures in `kws`.
    """
    
    if len(kws) == 0:
      if len(args) == 0:
        return Result()
      elif len(args) == 1:
        x = args[0]
        if isinstance(x, Future):
          return x
        elif isinstance(x, types_GeneratorType):
          return fresh(Coroutine(x))
        else:
          return Result(x)
    
    return fresh(All(
      *map(futurize, args),
      **dict((k, futurize(kws[k])) for k in kws)
    ))
  
  @export
  def coroutine(fn):
    """
    Convert the given generator function `fn` into a coroutine. The
    returned value is a future representing the last yielded value from
    `fn` or its raised exception. Intermediate yields from `fn` will be
    interpreted as futures (via `futurize`) and waited for non-blockingly.
    When an intermediate future is ready, the generator will be resumed
    with the single positional value of that future returned from the yield.
    """
    def fn1(*a,**kw):
      return fresh(Coroutine(fn(*a,**kw)))
    fn1.__name__ = fn.__name__
    fn1.__doc__ = fn.__doc__
    fn1.__wrapped__ = fn
    return fn1

  @export
  def jailed(fn, *args, **kws):
    """
    Immediately evaluate `fn(*args,**kws)`. If it returns a future
    then that is the return value of `jailed`. Otherwise, if `fn`
    returns a value it is returned as a singly-valued (Result) future.
    If `fn` raises, it is returned in an exceptional (Failure) future.
    """
    try:
      ans = fn(*args,**kws)
      if not isinstance(ans, Future):
        ans = Result(ans)
    except (AssertionError, KeyboardInterrupt):
      raise
    except BaseException as e:
      ans = Failure(e)
    return ans

  @export
  def when_done(*args):
    """
    Return a no-valued future representing the readiness of the given
    arguments. Any non-future arguments are considered immediately ready.
    """
    return fresh(WhenDone(futurize(a) for a in args))
  
  @export
  def when_succeeded(*args):
    """
    Return a no-valued future representing the successful readiness of
    the given arguments. If any arguments are futures representing
    exceptional values then this future will take on one of those
    exceptional values.
    """
    return fresh(WhenSucceeded([futurize(a) for a in args]))
  
  @export
  def mbind(*args, **kws):
    """
    Futurize the given arguments, when they are ready apply the decorated
    function to them. The result of that function (lifted to a future
    if not already) will be assigned over the definition of the funciton.
    ```
    @mbind(1, 2)
    def foo(one, two):
      return ...
    # foo is now a future representing the eventual return of the
    # function that was assigned to `foo` at decoration time.
    ```
    """
    def proxy(fn):
      return fresh(Mbind(futurize(*args, **kws), fn, jailed=False))
    return proxy
  
  @export
  def mbind_jailed(*args, **kws):
    """
    Futurize the given arguments, when that future is ready apply the
    decorated function directly to it as a future (not unwrapping its
    values as arguments). The result of that function (lifted to a future
    if not already) will be assigned over the definition of the funciton.
    ```
    @mbind(1, 2)
    def result(one_and_two):
      return ...
    # result is now a future representing the eventual return of the
    # function that was assigned to `result` at decoration time.
    ```
    """
    def proxy(fn):
      return fresh(Mbind(futurize(*args, **kws), fn, jailed=True))
    return proxy
  
  @export
  def after(before, *args, **kws):
    """
    Immediately evaluate `before(*args,**kws)` capturing either its
    returned value or raised exception as a future if not a future
    already. If `before` did not return a future, then invoke the decorated
    function on its captured result future (values not unwrapped as
    arguments), with the return of that bound to the decorated name.
    If `before` returned a future, then the decorated name is bound to
    the future representing the eventual execution of the decorated
    function against the final state of `before's` returned future.
    ```
    @after(foo, 1, 2)
    def result(fut):
      assert isinstance(fut, (Result, Failure))
      return 3
    # result could be a non-future value, a future, or the decoration
    # could have raised an exception from either before or result.
    ```
    """
    def proxy(aft):
      try:
        ans = before(*args, **kws)
        if isinstance(ans, Future):
          return fresh(Mbind(ans, aft, jailed=True))
        else:
          return aft(Result(ans))
      except (AssertionError, KeyboardInterrupt):
        raise
      except BaseException as e:
        return aft(Failure(e))
    return proxy
  
  @export
  class CriticalSection(object):
    """
    A future-based lock for coroutines.
    """
    
    def __init__(me, threadsafe=True):
      """
      Construct the lock. If `threadsafe` is absent or true then
      the lock can be used by concurrent os threads, otherwise it may
      only used by coroutines executing on the same thread.
      """
      nop = lambda:None
      lock = threading.Lock() if threadsafe else None
      
      me._state = (
        lock.acquire if threadsafe else nop,
        lock.release if threadsafe else nop,
        None, # head
        None # tail
      )
    
    def acquire(me):
      """
      Request the eventual acquisition of the lock. The return value is
      a singly-valued future containing the `releaser` callback. When
      the future is ready, the caller has the lock. To release the lock,
      the caller must invoke the no-argument releaser callback delivered
      in the future.
      ```
      lock = CriticalSection()
      @coroutine
      def foo():
        # Before critical section
        # ...
        release = yield lock.acquire()
        # In critical section
        # ...
        release()
        # After critical section
        # ...
      """
      
      lock_acq, lock_rel, head, tail = me._state
      lock_acq()
      
      def make_releaser(head_expect):
        def releaser():
          lock_acq, lock_rel, head, tail = me._state
          lock_acq()
          
          assert head is head_expect
          
          prom1, head = head
          if head is None:
            tail = None
          
          prom1.satisfy(make_releaser(head))
          
          me._state = (lock_acq, lock_rel, head, tail)
          lock_rel()
        return releaser
      
      tail1 = [Promise(), None]
      if head is None:
        prom = Result(make_releaser(tail1))
        head = tail1
      else:
        prom, _ = tail
        tail[1] = tail1
      
      me._state = (lock_acq, lock_rel, head, tail1)
      lock_rel()
      
      return prom

_everything()
del _everything
