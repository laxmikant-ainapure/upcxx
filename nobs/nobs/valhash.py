"""
nobs.valhash: Tools for hashing arbitrary python values. Only acyclic
object graphs are accepted.
"""

digest_zero = '\0'*20

def _everything():
  import __builtin__
  from array import array
  import binascii
  from collections import deque
  import hashlib
  import struct
  import sys
  import types
  
  from . import async
  
  BaseException = __builtin__.BaseException
  int = __builtin__.int
  isinstance = __builtin__.isinstance
  len = __builtin__.len
  long = __builtin__.long
  getattr = __builtin__.getattr
  sorted = __builtin__.sorted
  str = __builtin__.str
  type = __builtin__.type
  zip = __builtin__.zip
  
  bin2hex = binascii.hexlify
  hex2bin = binascii.unhexlify
  
  hashlib_sha1 = hashlib.sha1
  
  struct_pack = struct.pack
  
  def export(fn):
    globals()[fn.__name__] = fn
    return fn
  
  @export
  def digest_sum(*xs):
    c = 0L
    for x in xs:
      c += long(bin2hex(x), 16)
    c = '%x' % c
    if len(c) > 40:
      c = c[-40:]
    c = '0'*(40-len(c)) + c
    return hex2bin(c)

  @export
  def digest_subtract(a, b):
    a = long(bin2hex(a), 16)
    b = long(bin2hex(b), 16)
    c = a + (1L<<128) - b
    c = '%x' % c
    if len(c) > 40:
      c = c[-40:]
    c = '0'*(40-len(c)) + c
    return hex2bin(c)
  
  @export
  def follow(x):
    def g(f):
      if not hasattr(f, '_valhash_follow'):
        setattr(f,'_valhash_follow',[])
      getattr(f,'_valhash_follow').append(x)
      return f
    return g
  
  
  eat_action = {}
  
  def f(up, s_append, s_extend, x):
    code = x.func_code
    cells = x.func_closure or ()
    follow = getattr(x, '_valhash_follow', ())
    up('fn.%x.%x.%x.%x.' % (len(code.co_code), len(code.co_consts or ()), len(cells), len(follow)))
    up(code.co_code)
    s_extend(code.co_consts or ())
    for cell in cells:
      s_append(cell.cell_contents)
    s_extend(follow)
  eat_action[type(f)] = f
  
  def f(up, s_append, s_extend, x):
    up('ls.%x.' % len(x))
    s_extend(x)
  eat_action[list] = f

  def f(up, s_append, s_extend, x):
    up('tp.%x.' % len(x))
    s_extend(x)
  eat_action[tuple] = f
  
  def f(up, s_append, s_extend, x):
    n = len(x)
    up('d.%x.' % n)
    if n != 0:
      kvs = x.items()
      kvs.sort()
      ks,vs = zip(*kvs)
      s_extend(ks)
      s_extend(vs)
  eat_action[dict] = f
  
  def f(up, s_append, s_extend, x):
    up('se.%x.' % len(x))
    s_extend(sorted(x))
  eat_action[set] = f
  
  def f(up, s_append, s_extend, x):
    up('fs.%x.' % len(x))
    s_extend(sorted(x))
  eat_action[frozenset] = f
  
  def f(up, s_append, s_extend, x):
    up('sz.%x.' % len(x))
    up(x)
  eat_action[str] = f
  
  def f(up, s_append, s_extend, x):
    up('by.%x.' % len(x))
    up(x)
  eat_action[bytearray] = f
  
  def f(up, s_append, s_extend, x):
    up('ar.%s.%x.' % (x.typecode, len(x)))
    up(buffer(x))
  eat_action[array] = f
  
  def f(up, s_append, s_extend, x):
    up('bu.%x.' % len(x))
    up(x)
  eat_action[buffer] = f
  
  def f(up, s_append, s_extend, x):
    up('i.%x.' % long(x))
  eat_action[int] = f

  def f(up, s_append, s_extend, x):
    up('lo.%x.' % long(x))
  eat_action[long] = f
  
  def f(up, s_append, s_extend, x):
    up('fo.')
    up(struct_pack('<d', x))
  eat_action[float] = f
  
  def f(up, s_append, s_extend, x):
    up('t.' if x else 'f.')
  eat_action[bool] = f
  
  def f(up, s_append, s_extend, x):
    up('n.')
  eat_action[type(None)] = f
  
  def f(up, s_append, s_extend, x):
    up('fures.')
    s_append(x._val_seq)
    s_append(x._val_kws)
  eat_action[type(async.Result)] = f
  
  def f(up, s_append, s_extend, x):
    up('fufail.')
    s_append(x.exception)
  eat_action[type(async.Failure)] = f
  
  def act_unk(up, s_append, s_extend, x):
    ty = type(x)
    ty_mod = ty.__module__
    ty_name = ty.__name__
    
    if isinstance(x, BaseException):
      up('ex.%s.%s.' % (ty_mod, ty_name))
    elif 1: #ty is getattr(sys.modules[ty_mod], ty_name, None):
      up('o.%s.%s.' % (ty_mod, ty_name))
      got = getattr(x, '__getstate__', up)
      if got is not up:
        up('s')
        s_append(got())
      else:
        got = getattr(x, '__dict__', up)
        if got is not up:
          up('d%x.' % len(got))
          kvs = got.items()
          if len(kvs) != 0:
            kvs.sort()
            ks, vs = zip(*kvs)
            up('.'.join(ks))
            up('.')
            s_extend(vs)
        else:
          slots = getattr(ty, '__slots__', ())
          up('f%x.' % len(slots))
          up('.'.join(slots))
          up('.')
          for slot in slots:
            got = getattr(x, slot, up)
            if got is not up:
              s_append(got)
    else:
      assert False
      #up('?')
  
  def unhashable(up, s_append, s_extend, x):
    name = getattr(x, '__name__', '')
    if name:
      name = ', name='+name
    raise TypeError('Unhashable type: '+str(type(x))+name)
  
  eat_action[types.GeneratorType] = unhashable
  eat_action[async.Future] = unhashable
  eat_action[async.Promise] = unhashable
  eat_action[async.Coroutine] = unhashable
  eat_action[async.Mbind] = unhashable
  eat_action[async.WhenDone] = unhashable
  eat_action[async.WhenSucceeded] = unhashable
  
  eat_action_get = eat_action.get
  
  @export
  def eat(h, *xs):
    """
    Given a `hashlib` hash object object `h`, feed the values from `xs`
    in a deeply traversing manner into the state of `h`.
    The hopeful outcome is that equivalence of hash values (modulo the
    very improbable event of a collision) implies the observational
    equivalence of values (via `==`).
    """
    xs = list(xs) # stack
    xs_len = xs.__len__
    xs_pop = xs.pop
    xs_append = xs.append
    xs_extend = xs.extend
    up = h.update
    
    while xs_len() != 0:
      x = xs_pop()
      eat_action_get(type(x), act_unk)(up, xs_append, xs_extend, x)
    
    return h
  
  @export
  def digest_of(*xs):
    """
    Produce a SHA1 digest of the given values.
    """
    return eat(hashlib_sha1(), *xs).digest()

_everything()
del _everything
