"""
nobs.subexec: Asynchronous execution of child processes.

Similar in function to the subprocess module. Completion of the child 
process is returned in a `nobs.async.Future`. Child processes are 
executed against pseudo-terminals to capture their ASCII color codes. 
Failure is reported to `nobs.errorlog`.
"""
def _everything():
  import __builtin__
  import fcntl
  import os
  import re
  import select
  import signal
  import struct
  import sys
  import threading
  import time
  
  from . import async
  from . import errorlog
  
  len = __builtin__.len
  
  def export(fn):
    globals()[fn.__name__] = fn
    return fn

  class Job(async.Job):
    def __init__(me):
      me.wait_n = 0
      me.outputs = {}
      me.io_done = threading.Lock()
      me.io_done.acquire()

    def satisfy(me):
      me.wait_n -= 1
      if me.wait_n == 0:
        me.io_done.release()
    
    def join(me):
      try:
        _, status = os.waitpid(me.pid, 0)
      except:
        me.cancel()
        raise
      
      me.io_done.acquire()

      for fd in me.fds:
        os.close(fd)
      
      return (status, me.outputs['stdout'], me.outputs['stderr'])

    def cancel(me):
      os.kill(me.pid, signal.SIGTERM)
  
  io_cond = threading.Condition(threading.Lock())
  io_r = {} # {fd:([buf],outname,job)}
  io_w = {} # {fd:(rev_bufs,job)}
  io_thread_box = [None]
  
  def io_thread_fn():
    io_cond.acquire()
    while 1:
      if 0 == len(io_r) + len(io_w):
        if io_thread_box[0] is None:
          break
        io_cond.wait()
        continue
      
      io_cond.release()
      
      fds_r = list(io_r.keys())
      fds_w = list(io_w.keys())
      fds_r, fds_w, _ = select.select(fds_r, fds_w, [])
      
      for fd in fds_r:
        try:
          buf = os.read(fd, 32<<10)
        except OSError:
          buf = ''

        chks, outname, job = io_r[fd]
        
        if len(buf) == 0:
          del io_r[fd]
          job.outputs[outname] = ''.join(chks)
          job.satisfy()
        else:
          chks.append(buf)

      for fd in fds_w:
        rev_bufs, job = io_w[fd]
        os.write(fd, rev_bufs.pop())
        if len(rev_bufs) == 0:
          del io_w[fd]
          job.satisfy()

      io_cond.acquire()
    io_cond.release()
  
  def initialize():
    if io_thread_box[0] is None:
      with io_cond:
        if io_thread_box[0] is None:
          def kill_all():
            with io_cond:
              t = io_thread_box[0]
              io_thread_box[0] = None
              io_cond.notify()
            t.join()
            
          errorlog.at_shutdown(kill_all)

          io_thread_box[0] = threading.Thread(target=io_thread_fn, args=())
          io_thread_box[0].daemon = True
          io_thread_box[0].start()

  @export
  @async.coroutine
  def launch(args, capture_stdout=False, stdin='', cwd=None, env=None):
    """
    Execute the `args` list of strings as a command with appropriate
    `os.execvp` variant. The stderr output of the process will be
    captured in a pseudo-terminal (not just a pipe) into a string
    buffer and logged with `errorlog` appropriately. If `capture_stdout`
    is `True`, then the child's stdout will be captured in a pipe and
    later returned, otherwise stdout will be intermingled with the
    logged stderr. If `cwd` is present and not `None` it is the
    directory path in which to execute the child. If `env` is present
    and not `None` it must a dectionary of string to string representing
    the environment in which to execute the child.
    
    This function returns a future representing the termination of
    the child process. If the child has a return code of zero, then the
    future will contain the empty or stdout string depending on the
    value of `capture_stdout`. If the return code is non-zero then the
    future will be exceptional of type `errorlog.LoggedError`, thus
    indicating an aborting execution.
    """

    @async.launched
    def go():
      initialize()
      
      if capture_stdout:
        pipe_r, pipe_w = os.pipe()
        set_nonblock(pipe_r)
      
      pid, ptfd = os.forkpty()
      
      if pid == 0: # i am child
        if capture_stdout:
          os.close(pipe_r)
          os.dup2(pipe_w, 1)
          os.close(pipe_w)

        child_close_fds()
        
        if cwd is not None:
          os.chdir(cwd)
        
        if env is not None:
          os.execvpe(args[0], args, env)
        else:
          os.execvp(args[0], args)
      else: # i am parent
        with io_cond:
          job = Job()
          job.pid = pid
          job.wait_n = 1
          
          if len(stdin) > 0:
            job.wait_n += 1
            io_w[ptfd] = (reversed_bufs(stdin), job)
            
          io_r[ptfd] = ([], 'stderr', job)
          job.fds = [ptfd]
          
          if capture_stdout:
            job.wait_n += 1
            os.close(pipe_w)
            io_r[pipe_r] = ([], 'stdout', job)
            job.fds.append(pipe_r)
          else:
            job.outputs['stdout'] = ''
          
          io_cond.notify()
        
        return job
  
    if cwd is not None:
      sys.stderr.write('(in '+ cwd + ')\n')
    sys.stderr.write(' '.join(args) + '\n\n')

    status, out, err = yield go()
    
    if len(err) != 0:
      errorlog.show(' '.join(args), err)
  
    if status == 0:
      yield out
    else:
      raise errorlog.LoggedError(' '.join(args), err)
  
  def child_close_fds():
    try:
      fds = os.listdir('/dev/fd')
    except:
      fds = range(100)
    
    for fd in fds:
      try: fd = int(fd)
      except ValueError: continue
      if fd > 2:
        try: os.close(fd)
        except OSError: pass

  def reversed_bufs(s):
    cn = select.PIPE_BUF
    n = (len(s) + cn-1)/cn
    ans = [None]*n
    i = 0
    while i < n:
      ans[i] = s[i*cn : (i+1)*cn]
      i += 1
    ans.reverse()
    return ans

  def set_nonblock(fd):
    fcntl.fcntl(fd, fcntl.F_SETFL,
      fcntl.fcntl(fd, fcntl.F_GETFL) | os.O_NONBLOCK
    )
  
_everything()
del _everything
