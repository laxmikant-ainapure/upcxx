"""
nobs.tool_main: Contains the top-level main() function for the
command-line tool. The first argv argument to this process (sys.argv[1])
will name the rule to invoke, with the following argv arguments used
as arguments to that rule.
"""

def main():
  import os
  import sys
  
  from . import async
  from . import errorlog
  from . import memodb
  from . import ruletree
  from . import subexec
  
  def parse_arg(s):
    try: return int(s)
    except ValueError: pass
    try: return float(s)
    except ValueError: pass
    return s
  
  def trim_doc(docstring):
    if not docstring:
      return ''
    # Convert tabs to spaces (following the normal Python rules)
    # and split into a list of lines:
    lines = docstring.expandtabs().splitlines()
    # Determine minimum indentation (first line doesn't count):
    indent = sys.maxint
    for line in lines[1:]:
      stripped = line.lstrip()
      if stripped:
        indent = min(indent, len(line) - len(stripped))
    # Remove indentation (first line is special):
    trimmed = [lines[0].strip()]
    if indent < sys.maxint:
      for line in lines[1:]:
        trimmed.append(line[indent:].rstrip())
    # Strip off trailing and leading blank lines:
    while trimmed and not trimmed[-1]:
      trimmed.pop()
    while trimmed and not trimmed[0]:
      trimmed.pop(0)
    # Return a single string:
    return '\n'.join(trimmed)
  
  path_root = os.environ.get('NOBS_ROOT', os.getcwd())
  
  db = memodb.MemoDb(path_root)
  hooks = ruletree.cli_hooks(path_root, db)
  
  cmd = sys.argv[1]
  
  if cmd == 'help':
    cmd = sys.argv[2]
    _, fn = hooks.get(cmd, (None,None))
    if fn is None:
      print 'Unknown cli-hook "%s".' % cmd
      return 1
    else:
      print trim_doc(fn.__doc__) or "No __doc__ string for '%s'." % fn.__name__
      return 0
  else:
    try:
      fn, _ = hooks[cmd]
    except KeyError:
      print 'Unknown cli-hook "%s".' % cmd
      return 1
    
    @async.mbind(fn(*map(parse_arg, sys.argv[2:])))
    def printed(ans):
      if isinstance(ans, basestring):
        sys.stdout.write(ans)
      elif isinstance(ans, (int, long, float, bool)):
        sys.stdout.write(str(ans))
      elif isinstance(ans, (tuple, list, set, frozenset)):
        sys.stdout.write(u'\n'.join(map(unicode, ans)) + u'\n')
      else:
        sys.stdout.write(repr(ans))
    
    try:
      printed.wait()
    except BaseException as e:
      errorlog.aborted(e)
  
  db.save()
  return 0
