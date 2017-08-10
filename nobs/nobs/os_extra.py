def _everything():
  import os
  import shutil
  import tempfile
  
  os_listdir = os.listdir
  os_path_exists = os.path.exists
  os_path_join = os.path.join
  os_path_normcase = os.path.normcase
  os_path_split = os.path.split
  os_remove = os.remove
  os_rmdir = os.rmdir
  
  shutil_copyfile = shutil.copyfile
  shutil_copymode = shutil.copymode
  
  def memoize(fn):
    memo = {}
    memo_get = memo.get
    def proxy(x):
      y = memo_get(x, memo_get)
      if y is memo_get:
        memo[x] = y = fn(x)
      return y
    proxy.__doc__ = fn.__doc__
    proxy.__name__ = fn.__name__
    proxy.__wrapped__ = getattr(fn, '__wrapped__', fn)
    return proxy
  
  def export(fn):
    globals()[fn.__name__] = fn
    return fn
  
  with tempfile.NamedTemporaryFile(prefix='TmP') as f:
    is_case_sensitive = not os_path_exists(f.name.lower())
  
  globals()['is_case_sensitive'] = is_case_sensitive
  
  @export
  @memoize
  def listdir(path):
    """A memoized version of `os.listdir`. More performant, but only useful
    if you don't expect the queried directory to change during this
    program's lifetime."""
    return os_listdir(path)
  
  @export
  def rmtree(path):
    """Delete file or directory-tree at path."""
    try:
      os_remove(path)
    except OSError as e:
      if e.errno == 21: # is a directory
        try:
          for f in os_listdir(path):
            rmtree(os_path_join(path, f))
          os_rmdir(path)
        except OSError:
          pass
  
  @export
  def link_or_copy(src, dst):
    try:
      os_link(src, dst)
    except OSError as e:
      if e.errno == 18: # cross-device link
        shutil_copyfile(src, dst)
        shutil_copymode(src, dst)
      else:
        raise e
  
  # Case-sensitive system:
  if is_case_sensitive:
    @export
    @memoize
    def exists(path):
      """A memoized version of `os.path.exists`. More performant, but
      only useful if you don't expect the existence of queried files
      to change during this program's lifetime."""
      return os_path_exists(path)
    
    @export
    def realcase(path):
      """Returns the real upper/lower caseing of the given filename as
      stored in the filesystem. Your system is case-sensitive so this
      is the identity function."""
      return path
  
  # Case-insensitive system:
  else:
    @export
    @memoize
    def exists(path):
      """A memoized version of `os.path.exists` that also respects
      correct upper/lower caseing in the filename (a mismatch in case
      reports as non-existence). More performant than `os.path.exists`,
      but only useful if you don't expect the existence of queried files
      to change during this program's lifetime."""
      
      head, tail = os_path_split(path)
      
      if head == path:
        return os_path_exists(path)
      
      if not exists(head):
        return False
      
      return tail in listdir(head)
    
      sibs = listdir(head)
    
    @export
    @memoize
    def realcase():
      """Returns the real upper/lower caseing of the given filename as
      stored in the filesystem."""
      
      head, tail = os_path_split(path)
      if head == path:
        return path
      else:
        sibs = listdir(head)
        sibmap = dict((os_path_normcase(sib), sib) for sib in sibs)
        normtail = os_path_normcase(tail)
        return os_path_join(realcase(head), sibmap.get(normtail, tail))

_everything()
del _everything
