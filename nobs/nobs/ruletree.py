"""
nobs.ruletree: Loading and evaluating no-bs rule files from a directory
tree.

A rule-file is a regular python file (but always named nobsrule.py) 
that is interpreted in a slightly modified context. The objective of a 
rule-file is to define "rule" functions. The meaning of a rule is 
entirely user defined, but they commonly compute metadata or perform 
heavyweight computations (like compilation) on files in the project 
directory tree. Rule functions must be idempodent pure functions. 
Rule-files have an inheritance model where rules in child directories 
override those in parent directories with a powerful method to defer 
computation to parent rules in a context preserving manner.

A rule-file has a number of predefined functions available in its
default global namespace. The most important of which are `rule` and
`rule_memoized` for declaring rules. An example project demonstrating
typical rule-file structure:

```
# The directoy structure for our example project:
# project/
#   nobsrule.py
#   main.c
#   kernel/
#     nobsrule.py
#     crunch.c
#     setup.c

# -- In project/nobsrule.py --------------------------------------------

@rule(path_arg='src')
def optimization_flags(cxt, src):
  return ['-O2']

# -- In project/kernel/nobsrule.py -------------------------------------

@rule()
def optimization_flags(cxt, src):
  if src == here('crunch.c'):
    return ['-O3','-ffast-math']
  else:
    return cxt.optimization_flags(src)
```

In the top-level rule-file "project/nobsrule.py" we define the rule 
`optimization_flags` using the `@rule` decorator (implicitly available 
in the global namespace). The rule is then overriden in the child 
rule-file "project/kernel/nobsrule.py". In the top-most definition, the 
`path_arg` is set to `'src'` to indicate that this argument be 
considered as the filesystem path inheritance should follow when 
evaluating rules as specific to files. This means that when the system 
evaluates calls like `optimization_flags(my_src)`, it will prefer 
definitions of `optimization_flags` in rule-files closest to `my_src` 
in the directoy nesting.

All rule functions take as their first argument a context argument 
which will allow them to query other rules in the course of their 
evaluation. Calling "naked" rules as free functions instead of as 
members of the context is not permitted. Deferring implementation to 
parent rule-files is accomplished by invoking infinite-recursion. See 
that the rule in "kernel/nobsrule.py" calls itself with the same 
arguments it was given in the else branch, the runtime recognizes this 
as a cyclic call and defers one level up in the rule-file nesting for 
the recursive invocation.

To demonstrate the power of rule inheritance, we will add an additional
rule to the project/nobsrule.py file which will generate the compilation
command specific to a given source file:
```
# -- In project/nobsrule.py --------------------------------------------
@rule(path_arg='src')
def compiler(cxt, src):
  return ['gcc','-Wall'] + cxt.optimization_flags(src) + ['-c',src]

# -- example evaluations of compiler() ---------------------------------

compiler('main.c')
# return: ['gcc','-Wall','-O2','-c','main.c']

compiler('kernel/setup.c')
# return: ['gcc','-Wall','-O2','-c','kernel/setup.c']

compiler('kernel/crunch.c')
# return: ['gcc','-Wall','-O3','-ffast-math','-c','kernel/crunch.c']
```

Memoization of heavyweight rules is achieved by declaring them with the 
`rule_memozied` decorator. As opposed to `@rule`, the syntactic 
construct of decoration is no longer a function but a python class, 
though invoking the rule will still behave as function-like. The 
decorated class must have one `execute` method, and usually at least 
one method declared with the `traced` decorator. The `execute` method 
is the function which will run if the memoization database does not 
contain an entry for this work having been previously done. The traced 
functions are how `execute` can access the arguments it was called 
against (execute itself takes only a single `me` memoization context 
object argument). Traced functions receive a memoization `me` object, 
the rule-file context object `cxt`, the arguments supplied at rule 
invocation, and the arguments supplied by invocation of the trace 
function, in that order.  The runtime intercepts calls to traced 
functions to record them as dependencies. Traced functions are 
restricted to having only simple types for their arguments and return 
value (scalars, lists, tuples, dicts, sets, and frozensets of simple 
types, and carefully crafted lambdas closing over simple types).

```
@rule_memoized(path_arg=0) # use the first positional argument from rule
                           # invocation for inheritance search (this is
                           # called src in get_compiler trace function)
class compiled:
  # @traced
  # def example_trace(me, cxt, <rule-arguments...>, <trace-arguments...>):
  #   return ...
  
  @traced
  def get_compiler(me, cxt, src): # src is from rule invocation,
                                  # no args from trace invocation
    return cxt.compiler(src)
  
  @coroutine
  def execute(me):
    compiler = me.get_compiler() # no trace invocation arguments
    
    # ask for a new filename to produce as output of the rule
    ofile = me.mkpath(key=None)
    
    # compile src to ofile
    yield subexec.launch(compiler+['-o,ofile])
    
    # return ofile
    yield ofile

@coroutine
def some_outer_rule(cxt):
  # compile "main.c", receive path to object file as return
  main_o = yield cxt.compiled('main.c')
  ...
```

The list of implicitly available global definitions for a rule-file is:
  
  async: Implicit import of the `nobs.async` module.
  
  coroutine: See `async.coroutine`. Decorates a generator function for 
  use as a coroutine. Yielded futures will be waited for. The last 
  yield will be the return of the coroutine. Invocation of the 
  decorated function will return a future proxying the final yield of 
  the generator.
  
  digest_of: See `valhash.digest_of`. Hashes generic (but acyclic) 
  python values into 20-byte SHA1 hashes.
  
  futurize: See `async.futurize`. Creates a future from argument(s) if 
  not already a future.
  
  here: returns os.path.join of absolute path to directory containing 
  this rule-fule with supplied argument list.
  
  cached: Decorates a function for short-lived memoization that only 
  lives as long the process.
  
  rule: Decorates a function as a rule.
  
  rule_memoized: Decorates a class as a long-term memoized rule 
  (survives across process lifetimes).
  
  traced: Decorates a method in a memoization class as a traced 
  function.
"""
def _everything():
  import __builtin__
  import binascii
  import bisect
  import os
  import sys
  import types
  
  from . import async
  from . import memodb
  from . import valhash
  
  def export(fn):
    globals()[fn.__name__] = fn
    return fn
  
  __import__ = __builtin__.__import__
  AttributeError = __builtin__.AttributeError
  execfile = __builtin__.execfile
  float = __builtin__.float
  dict = __builtin__.dict
  getattr = __builtin__.getattr
  int = __builtin__.int
  isinstance = __builtin__.isinstance
  IOError = __builtin__.IOError
  KeyError = __builtin__.KeyError
  len = __builtin__.len
  list = __builtin__.list
  NameError = __builtin__.NameError
  object = __builtin__.object
  open = __builtin__.open
  setattr = __builtin__.setattr
  str = __builtin__.str
  ValueError = __builtin__.ValueError
  zip = __builtin__.zip
  
  async_coroutine = async.coroutine
  
  bisect_left = bisect.bisect_left
  
  hexlify = binascii.hexlify
  
  os_path_abspath = os.path.abspath
  os_path_dirname = os.path.dirname
  os_path_exists = os.path.exists
  os_path_join = os.path.join
  os_path_relpath = os.path.relpath
  os_path_samefile = os.path.samefile
  
  types_FunctionType = types.FunctionType
  types_ModuleType = types.ModuleType
  
  digest_of = valhash.digest_of
  
  def import_interposer(name, globals={}, locals={}, fromlist=[], level=-1):
    here = globals.get('__nobs_here__')
    if here is not None:
      old_sys_path = sys.path
      sys.path = [here] + old_sys_path
      mod = __import__(name, globals, locals, fromlist, level)
      sys.path = old_sys_path
    else:
      mod = __import__(name, globals, locals, fromlist, level)
    return mod
  
  __builtin__.__dict__['__import__'] = import_interposer
  
  @export
  def cached(fn):
    """
    Decorate `fn` for short-lived memoization which does not outlive
    the process lifetime.
    """
    xs = []
    xs_len = xs.__len__
    ys = []
    def proxy(*a, **kw):
      x = (a, kw)
      i = bisect_left(xs, x)
      if i < xs_len() and xs[i] == x:
        return ys[i]
      else:
        y = fn(*a, **kw)
        xs.insert(i, x)
        ys.insert(i, y)
        return y
    proxy.__name__ = fn.__name__
    proxy.__doc__ = fn.__doc__
    proxy.__wrapped__ = fn
    return proxy
  
  @export
  def rule(cli=None, path_arg=None):
    """
    Create a decorator which will convert its function into a rule. If
    `cli` is not `None` than this rule will be invocable from the "tool.py"
    command-line with the value as its name. If `path_arg` is not `None`
    then the value supplied at rule invocation time corresponding to the
    argument of that name (if string) or position (if integer) will be
    used for rule-file inheritance.
    """
    def decorator(fn):
      def dummy(*a,**kw):
        raise Exception("Rule must be invoked as a method on a context object.")
      
      dummy.__doc__ = fn.__doc__
      dummy.__name__ = fn.__name__
      dummy.__wrapped__ = getattr(fn, '__wrapped__', fn)
      dummy._ruletree_fn = fn
      dummy._ruletree_cli = cli
      
      arg = path_arg
      
      if isinstance(arg, basestring):
        try:
          wrapped = getattr(fn, '__wrapped__', fn)
          arg = -1 + wrapped.func_code.co_varnames.index(arg)
        except ValueError:
          pass
      
      if isinstance(arg, basestring):
        dummy._ruletree_patharg = lambda a,kw: kw[arg]
        def absify(a, kw):
          kw1 = dict(kw)
          kw1[arg] = os_path_abspath(kw[arg])
          return (a, kw1)
      elif type(arg) in (int, long):
        dummy._ruletree_patharg = lambda a,kw: a[arg]
        def absify(a, kw):
          a1 = list(a)
          a1[arg] = os_path_abspath(a[arg])
          return (a1, kw)
      else:
        dummy._ruletree_patharg = None
        absify = lambda a,kw:(a,kw)
      
      dummy._ruletree_absify = absify
      return dummy
    return decorator
  
  @export
  def traced(fn):
    """
    Decorates `fn` as a traced method in a memoized-rule class. `fn`
    should accept a memoization context object, a rule-file context object,
    the rule-invocation arguments, and the trace invocation arguments
    all as one long argument list in that order.
    
    The methods available on memoization context objects for traced 
    functions are as follows:
    
      <<name of traced function>>(*trace_args):
        Calls the traced function of the given name with `trace_args` as
        the trace invocation arguments. Returns the trace function's
        result.
      
      depend_files(*paths):
        Registers the given file paths as memoization dependencies.
        Changes in the contents of these files will trigger re-execution
        of the rule.
      
      depend_fact(key, value):
        Register the `key,value` pair as a dependency of this rule.
        Detected changes in values will trigger re-execution of the rule.
      
      depend_facts(facts_dict):
        Register each `key,value` fact in the `facts_dict` dictionary
        using `depend_fact`.
    """
    
    def proxy(dbcxt, rcxt, *a):
      return fn(dbcxt, rcxt._with_dbcxt(dbcxt), *a)
    proxy.__name__ = fn.__name__
    proxy.__doc__ = fn.__doc__
    proxy.__wrapped__ = getattr(fn, '__wrapped__', fn)
    
    return memodb.traced(proxy)
  
  @export
  def rule_memoized(cli=None, path_arg=None):
    """
    Decorates the class `cls` for use as a memoized rule. If `cli` is
    not `None` then this rule will be invocable from the command-line
    program "tool.py". If `path_arg` is not `None` then it must be an
    integer indicating the position of the rule invocation argument to
    use for inheritance search.
    
    The class `cls` must contain one `execute(me)` method with `me` as 
    a memoization context object. Memoization context objects given to
    `execute` have the following methods:
    
      <<name of traced function>>(*trace_args):
        Calls the traced function of the given name with `trace_args` as
        the trace invocation arguments. Returns the traced function's
        result.
      
      depend_files(*paths):
        Registers the given file paths as memoization dependencies.
        Changes in the contents of these files will trigger re-evaluation
        of the rule.
        
      mkpath(key, prefix='', suffix=''):
        Returns a generated filename for storing output of this rule 
        (generated files). `key` should be some generic python value 
        distinguishing this file from other invocations of `mkpath` 
        within the same context (typically `None` if mkpath is only 
        called once in its context). `prefix` and `suffix` are optional 
        strings to occur at the front or back of the filename, they 
        must not contain directory separation slashes.
      
      mktree(entries, symlinks):
        Creates a directory tree from the `entries` dictionary. For 
        each `name,value` pair in `entries` prodcues a directory entry 
        which is either a file or another directory. If `value` is a 
        string and names a path to an existing file, then this entry 
        represents a hard-link or copy of that file (if symlinks=False) 
        or a sym-link to that file (if symlinks=True). If `value` is a 
        string which names an existing directory then this entry will 
        either be a symlink to or a deep-copy of (possibly with 
        hard-links for the files) that directory depedning on the value 
        of `symlinks`. If 'value' is a string not corresponding to an 
        existing filesystem object, then a (broken) sym-link to that 
        path is generated. The path to the directory tree is returned.
      
      mkdtemp():
        Returns a path to a directory constructed for use temporary use
        during the lifetime of the memoized rule's `execute` method.
        This directory tree will be deleted automatically after
        `execute` returns.
    """
    def decorator(cls):
      mem = memodb.memoized(cls)
      
      def proxy(rcxt, *a):
        return mem(rcxt._dbcxt, rcxt, *a)
      proxy.__name__ = cls.__name__
      proxy.__doc__ = cls.__doc__
      proxy.__wrapped__ = getattr(cls, '__wrapped__', cls)
      
      return rule(cli=cli, path_arg=path_arg)(proxy)
    return decorator
  
  @export
  def cli_hooks(path_root, memo_db):
    """
    Load the rule-file tree rooted at `path_root` using the supplied 
    MemoDb instance `memo_db` for caching memoized rules. Returns a 
    dictionary mapping command-line names to callable context-bound 
    rules.
    """
    path_root = os_path_abspath(path_root)
    node_cache = {}
    node_cache_get = node_cache.get
    node_root = None # defined after node_at()
    node_empty = (None, {}, {}.get, {}, {}.get)
    
    # Node: (parent, defs, defs.get, clis, clis.get),
    #   parent: Node
    #   defs: {name: (value, owner_node)}
    #   clis: {cliname: (defname, ancestor_value)}
    
    def node_at(path):
      path = os_path_abspath(path)
      
      node = node_cache_get(path, node_cache_get)
      if node is not node_cache_get:
        return node
      
      def slashed(path):
        slash = os.path.sep
        return path + slash*(not path.endswith(slash))
      
      if not slashed(path).startswith(slashed(path_root)):
        return node_root
      
      try:
        same_as_root = os_path_samefile(path_root, path)
      except OSError:
        same_as_root = False
      
      if not same_as_root:
        parent = node_at(os_path_dirname(path))
      else:
        nil = {}
        parent = node_empty
      
      path_rule = os_path_join(path, 'nobsrule.py')
      try:
        with open(path_rule, 'r'): pass
        readable = True
      except IOError:
        readable = False
      
      if not readable:
        node = parent
      else:
        modname = 'nobsrule_' + hexlify(digest_of(path_rule))
        mod = types_ModuleType(modname)
        mod.__file__ = path_rule
        mod.__package__ = None
        sys.modules[modname] = mod
        
        defs = mod.__dict__
        defs.update({
          #'__builtin__': bltin,
          #'__builtins__':bltin.__dict__,
          '__nobs_here__': path,
          'async': async,
          'cached': cached,
          'coroutine': async_coroutine,
          'digest_of': digest_of,
          'futurize': async.futurize,
          'here': lambda *xs: os_path_join(path, *xs),
          'rule': rule,
          'rule_memoized': rule_memoized,
          'traced': traced
        })
        execfile(path_rule, defs)
        
        _, parent_defs, _, parent_clis, _ = parent
        
        clis = dict(parent_clis)
        
        for x,y in defs.iteritems():
          if not x.startswith('__'):
            patharg = getattr(y, '_ruletree_patharg', None)
            cli = getattr(y, '_ruletree_cli', None)
            
            parent_y, parent_nd = parent_defs.get(x, (None,None))
            
            if parent_nd is not None:
              parent_patharg = getattr(y, '_ruletree_patharg', None)
              if patharg is not None and patharg != parent_patharg:
                raise Exception("Child definition '%s:%s' may not change 'path_arg' annotation." % (path_rule,x))
              if patharg is None:
                y._ruletree_patharg = parent_patharg
              
              parent_cli = getattr(y, '_ruletree_cli', None)
              if cli is not None and cli != parent_cli:
                raise Exception("Child definition '%s:%s' may not change 'cli' annotation." % (path_rule,x))
              if cli is None:
                y._ruletree_cli = parent_cli
            
            if cli is not None and x not in clis:
              clis[cli] = (x, y)
        
        defs1 = dict(parent_defs)
        node = (parent, defs1, defs1.get, clis, clis.get)
        defs1.update(dict((x,(y,node)) for x,y in defs.iteritems()))
      
      node_cache[path] = node
      return node
      
    class Context(object):
      def __init__(me, rootnode, nodemap, dbcxt):
        _, rootdefs, rootdefs_get, rootclis, rootclis_get = rootnode
        
        me._dbcxt = dbcxt
        me__dict__ = me.__dict__
        
        def _getattr(x):
          y_and_nd = rootdefs_get(x)
          if y_and_nd is None:
            raise AttributeError("No definition for %s" % x)
          y, _ = y_and_nd
          ans = _wrap(x, y)
          me__dict__[x] = ans
          return ans
        # register method
        me._getattr = _getattr
        
        def _with_dbcxt(dbcxt1):
          return Context(rootnode, nodemap, dbcxt1)
        # register method
        me._with_dbcxt = _with_dbcxt
        
        def _wrap(x, y):
          if getattr(y, '_ruletree_fn', None) is None:
            return y
          else:
            patharg = y._ruletree_patharg
            
            def proxy(*a, **kw):
              key = (x, a, kw)
              nd = Map_get(nodemap, key, nodemap)
              if nd is nodemap:
                if patharg is not None:
                  nd = node_at(patharg(a, kw))
                else:
                  nd = rootnode
              
              _, _, nd_defs_get, _, _ = nd
              y_nd = nd_defs_get(x, nd_defs_get)
              
              if y_nd is nd_defs_get:
                if patharg is not None:
                  raise NameError(
"No definition for '"+x+"' found. Make sure a rule-definition exists in " +
"'"+os.path.join(path_root, 'nobsrule.py')+"' or in a 'nobsrule.py' " +
"within that directory tree along the path to '"+patharg(a,kw)+"'."
                  )
                else:
                  raise NameError("No definition for '%s'"%x)
              
              y, nd = y_nd
              nd_parent, _, _, _, _ = nd
              
              if isinstance(y, types_FunctionType):
                return getattr(y, '_ruletree_fn', y)(
                  Context(nd, Map_with_put(nodemap, key, nd_parent), dbcxt),
                  *a, **kw
                )
              else:
                return y
            
            proxy.__name__ = y.__name__
            proxy.__doc__ = y.__doc__
            proxy.__wrapped__ = y._ruletree_fn
            return proxy
        # register method
        me._wrap = _wrap
      
      def __getattr__(me, x):
        return me._getattr(x)
    
    ####################################################################
    # body logic of cli_hooks():
    node_root = node_at(path_root)
    _, _, _, root_clis, _, = node_root
    cxt_root = Context(node_root, Map_empty(), memo_db)
    
    def wrap_hook(x, y0):
      w = cxt_root._wrap(x, y0)
      def absified(*a,**kw):
        a,kw = y0._ruletree_absify(a,kw)
        return w(*a,**kw)
      return (absified, y0)
    
    return dict((cmd, wrap_hook(x,y0)) for cmd,(x,y0) in root_clis.items())
  
  def Map_empty():
    return (0, [], [])
  
  def Map_with_put(m, k, v):
    n, ks, vs = m
    i = bisect_left(ks, k)
    if i < n and ks[i] == k:
      vs1 = list(vs)
      vs1[i] = v
      return (n, ks, vs1)
    else:
      ks1 = list(ks); ks1.insert(i, k)
      vs1 = list(vs); vs1.insert(i, v)
      return (n+1, ks1, vs1)
  
  def Map_get(m, k, v_otherwise=None):
    n, ks, vs = m
    i = bisect_left(ks, k)
    if i < n and ks[i] == k:
      return vs[i]
    else:
      return v_otherwise

_everything()
del _everything
