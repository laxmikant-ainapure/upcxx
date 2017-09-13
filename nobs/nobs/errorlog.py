"""
nobs.errorlog: Logs errors during the course of process execution.
"""
def _everything():
  import __builtin__
  import hashlib
  import os
  import sys
  
  from . import async
  
  len = __builtin__.len
  
  def export(fn):
    globals()[fn.__name__] = fn
    return fn
  
  RESET = '\x1b[0m'
  RED = '\x1b[31m'
  YELLOW = '\x1b[33m'
  BLUE = '\x1b[34m'
  WHITE = '\x1b[37m'

  if os.environ.get('TERM','') == 'dumb':
    RESET = ''
    RED = ''
    YELLOW = ''
    BLUE = ''
    WHITE = ''
  
  def terminal_rows_cols():
    import struct
    
    def ioctl_GWINSZ(fd):
      try:
        import fcntl
        import termios
        return struct.unpack('hh', fcntl.ioctl(fd, termios.TIOCGWINSZ, '1234'))
      except:
        return None
    
    ans = ioctl_GWINSZ(0) or ioctl_GWINSZ(1) or ioctl_GWINSZ(2)
    
    if ans is None:
      try:
        term_id = os.ctermid()
        try:
          fd = os.open(term_id, os.O_RDONLY)
          ans = ioctl_GWINSZ(fd)
        finally:
          os.close(fd)
      except:
        pass
    
    if ans is None:
      try:
        with os.popen('stty size', 'r') as f:
          ans = f.read().split()
      except:
        pass
    
    if ans is None:
      return None
    ans = map(int, ans)
    if ans[0] <= 0 or ans[1] <= 0:
      return None
    return ans
  
  isatty = sys.stdout.isatty() and sys.stderr.isatty()
  t_rows_cols = terminal_rows_cols() if isatty else None
  isatty = isatty and t_rows_cols is not None
  
  if not isatty:
    t_rows, t_cols = 25, 50
  else:
    t_rows, t_cols = t_rows_cols
  
  BAR = '~'*t_cols
  
  _fatal = [None]
  _log = [] # [(title,message)]
  
  @export
  def raise_if_aborting():
    """
    If a LoggedError has been previously raised, then re-raise it here.
    Otherwise no-op.
    """
    e = _fatal[0]
    if e is not None:
      raise e
  
  @export
  class LoggedError(Exception):
    """
    An exception which will be logged and displayed at shutdown.
    Constructing an instance of this class, even if the instance is
    never raised, logs the error message for later display and marks
    the whole execution as aborted.
    """
    def __init__(me, title, message=''):
      _fatal[0] = me
      pair = (title, message)
      
      if pair not in _log:
        _log.append(pair)
        
        if isatty:
          if _log[0] is pair:
            sys.stderr.write(RED + '*** Something FAILED! ***' + RESET + '\n')
        else:
          sys.stderr.write(''.join([
            '~'*50, '\n',
            RED + title + RESET,
            '\n\n'*(message != ''),
            message,
            (not message.endswith('\n'))*'\n'
          ]))
  
  @export
  def warning(message):
    show("WARNING: " + message)
  
  @export
  def show(title, message):
    """
    Print error message to stderr and display in the abort log if an
    aborting error occurs elsewhere.
    """
    pair = (title, message)
    
    if pair not in _log:
      _log.append((title, message))
      
      sys.stderr.write(''.join([
        BAR, '\n',
        RED + title + RESET,
        '\n\n'*(message != ''),
        message,
        '\n'*(not message.endswith('\n')),
        #BAR, '\n'
      ]))
  
  @export
  def aborted(exception, tb=None):
    """
    Called from the top-level main() code to indicate that an uncaught
    exception was about to abort execution. This call will then display
    the error log and invoke sys.exit(1).
    """
    import re
    from traceback import format_exception
    
    if isinstance(exception, KeyboardInterrupt):
      sys.exit(1)
    elif not isatty:
      if exception is not None and not isinstance(exception, LoggedError):
        if tb is None:
          tb = sys.exc_info()[2]
        
        message = ''.join(format_exception(type(exception), exception, tb))
        
        BAR = 50*'~'
        sys.stderr.write(''.join([
          BAR, '\n',
          RED + 'Uncaught exception' + RESET,
          '\n\n'*(message != ''),
          message,
          '\n'*(not message.endswith('\n')),
          #BAR, '\n'
        ]))
      
      sys.exit(1)
      
    else:
      t_rows, t_cols = terminal_rows_cols()
      BAR = '~'*t_cols
      
      if exception is not None and not isinstance(exception, LoggedError):
        from traceback import format_exception
        if tb is None:
          tb = sys.exc_info()[2]
        _log.append((
          'Uncaught exception',
          ''.join(format_exception(type(exception), exception, tb))
        ))
      
      text = '\n'.join(
        ''.join([
          BAR, '\n',
          RED, title, RESET,
          '\n\n'*(message!=''),
          message,
          '\n'*(not message.endswith('\n'))
        ])
        for title,message in _log
      )
      
      text_plain = re.sub(r'(\x9b|\x1b\[)[0-?]*[ -\/]*[@-~]', '', text)
      text_rows = sum(map(lambda line: (len(line)+t_cols-1)/t_cols, text_plain.split('\n')))
      
      if text_rows >= t_rows-3:
        import subprocess as sp
        pager = os.environ.get('PAGER','less -R').split()
        less = sp.Popen(pager, stdin=sp.PIPE)
        less.communicate(text)
      else:
        sys.stderr.write(text)
      sys.exit(1)

_everything()
del _everything
