def _everything():
  import os
  import shutil
  import tempfile
  
  os_link = os.link
  os_listdir = os.listdir
  os_makedirs = os.makedirs
  os_path_dirname = os.path.dirname
  os_path_exists = os.path.exists
  os_path_isdir = os.path.isdir
  os_path_isfile = os.path.isfile
  os_path_join = os.path.join
  os_path_normcase = os.path.normcase
  os_path_split = os.path.split
  os_remove = os.remove
  os_rmdir = os.rmdir
  os_symlink = os.symlink
  
  shutil_copyfile = shutil.copyfile
  shutil_copymode = shutil.copymode
  
  def memoize(fn):
    memo = {}
    memo_get = memo.get
    def proxy(x):
      y = memo_get(x, memo_get)
      if y is memo_get:
        y = fn(x)
        memo[x] = y
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
  def isfile(path):
    return os_path_isfile(path)
  
  @export
  @memoize
  def isdir(path):
    return os_path_isdir(path)
  
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
  def link_or_copy(src, dst, overwrite=False):
    try:
      try:
        os_link(src, dst)
      except OSError as e:
        if e.errno == 18: # cross-device link
          shutil_copyfile(src, dst)
          shutil_copymode(src, dst)
        else:
          raise
    except OSError as e:
      if e.errno == 17 and overwrite: # File exists
        rmtree(dst)
        link_or_copy(src, dst)
      else:
        raise
  
  @export
  def ensure_dirs(path):
    d = os_path_dirname(path)
    if not os_path_exists(d):
      os_makedirs(d)
  
  @export
  def mktree(path, entries, symlinks=True):
    def enter(path, entries):
      try: os_makedirs(path)
      except OSError: pass
      
      for e_name, e_val in entries.items():
        path_and_name = os_path_join(path, e_name)
        
        if isinstance(e_val, dict):
          enter(path_and_name, e_val)
        else:
          if symlinks:
            os_symlink(e_val, path_and_name)
          else:
            if os_path_isfile(e_val):
              link_or_copy(e_val, path_and_name)
            elif os_path_isdir(e_val):
              enter(
                path_and_name,
                dict((nm, os_path_join(e_val,nm)) for nm in listdir(e_val))
              )
            elif os_path_islink(e_val):
              target = os_readlink(e_val)
              os_symlink(target, path_and_name)
    
    enter(path, entries)
  
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
      
      if head != '' and not exists(head):
        return False
      
      return tail == '' or tail in listdir(head or '.')
    
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
