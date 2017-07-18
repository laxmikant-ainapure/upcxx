"""
nobs.subexec: Asynchronous execution of child processes.

Similar in function to the subprocess module. Completion of the child 
process is returned in a `nobs.async.Future`. Child processes are 
executed against pseudo-terminals to capture their ASCII color codes. 
Failure is reported to `nobs.errorlog`.
"""
def _everything():
  import __builtin__
  import os
  import re
  import struct
  import sys
  import threading
  
  from . import async
  from . import errorlog
  
  len = __builtin__.len
  
  def export(fn):
    globals()[fn.__name__] = fn
    return fn
  
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
      errorlog.raise_if_aborting()
      
      if False:
        import subprocess as sp
        p = sp.Popen(args, stdout=sp.PIPE, stderr=sp.PIPE, stdin=sp.PIPE, cwd=cwd, env=env)
        out, err = p.communicate(stdin)
        rc = p.returncode
      else:
        if capture_stdout:
          pipe_r, pipe_w = os.pipe()
        
        pid, ptfd = os.forkpty()
        
        if pid == 0: # i am child
          if capture_stdout:
            os.close(pipe_r)
            os.dup2(pipe_w, 1)
            os.close(pipe_w)
          
          if cwd is not None:
            os.chdir(cwd)
          
          if env is not None:
            os.execvpe(args[0], args, env)
          else:
            os.execvp(args[0], args)
        else:
          if capture_stdout:
            os.close(pipe_w)
          
          def start_reader(fd):
            bufs = []
            def reader():
              while True:
                try:
                  buf = os.read(fd, 8192)
                except OSError:
                  buf = ''
                if len(buf) == 0: break
                bufs.append(buf)
            t = threading.Thread(target=reader, args=())
            t.daemon = True
            t.start()
            def joiner():
              t.join()
              return ''.join(bufs)
            return joiner
          
          os.write(ptfd, stdin)
          t0 = start_reader(ptfd)
          if capture_stdout:
            t1 = start_reader(pipe_r)
          
          err = t0()
          os.close(ptfd)
          
          if capture_stdout:
            out = t1()
            os.close(pipe_r)
          else:
            out = ''
          
          _, rc = os.waitpid(pid, 0)
      
      if rc == 0:
        return (out, err)
      else:
        raise errorlog.LoggedError(' '.join(args), err)
    
    if cwd is not None:
      sys.stderr.write('(in '+ cwd + ')\n')
    sys.stderr.write(' '.join(args) + '\n\n')
    
    out, err = yield go()
    
    if len(err) != 0:
      errorlog.show(' '.join(args), err)
    
    yield out

_everything()
del _everything
