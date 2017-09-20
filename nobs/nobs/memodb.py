"""
nobs.memodb: Memoization of asynchronous functions to a database file.

This API is mostly wrapped for the user by the `nobs.ruletree` module.
See that module for documentation of memoization context objects.
"""
def _everything():
  # Useful in debugging to leave temporary files/dirs in tact after
  # nobs exits so they may be autopsied.
  DONT_REMOVE_TEMPS = False

  import __builtin__
  import binascii
  import cPickle
  import hashlib
  import os
  import sys
  import types
  
  from . import async
  from . import valhash
  from . import os_extra
  
  def export(x):
    globals()[x.__name__] = x
    return x
  
  AssertionError = __builtin__.AssertionError
  BaseException = __builtin__.BaseException
  dict = __builtin__.dict
  dir = __builtin__.dir
  Exception = __builtin__.Exception
  getattr = __builtin__.getattr
  isinstance = __builtin__.isinstance
  iter = __builtin__.iter
  KeyError = __builtin__.KeyError
  len = __builtin__.len
  list = __builtin__.list
  map = __builtin__.map
  object = __builtin__.object
  open = __builtin__.open
  OSError = __builtin__.OSError
  set = __builtin__.set
  sorted = __builtin__.sorted
  str = __builtin__.str
  super = __builtin__.super
  TypeError = __builtin__.TypeError
  ValueError = __builtin__.ValueError
  zip = __builtin__.zip
  
  types_FunctionType = types.FunctionType
  types_MethodType = types.MethodType
  
  os_link = os.link
  os_listdir = os.listdir
  os_makedirs = os.makedirs
  os_remove = os.remove
  os_rmdir = os.rmdir
  os_symlink = os.symlink
  os_walk = os.walk
  
  os_path_abspath = os.path.abspath
  os_path_join = os.path.join
  os_path_normpath = os.path.normpath
  os_path_sep = os.path.sep
  
  cPickle_load = cPickle.load
  cPickle_dump = cPickle.dump
  cPickle_dumps = cPickle.dumps
  
  hexlify = binascii.hexlify
  
  async_after = async.after
  async_Failure = async.Failure
  async_Future = async.Future
  async_coroutine = async.coroutine
  async_jailed = async.jailed
  async_mbind = async.mbind
  async_mbind_jailed = async.mbind_jailed
  async_Promise = async.Promise
  async_Result = async.Result
  async_when_done = async.when_done
  
  hashlib_sha1 = hashlib.sha1
  
  os_extra_exists = os_extra.exists
  os_extra_link_or_copy = os_extra.link_or_copy
  os_extra_rmtree = os_extra.rmtree
  
  valhash_eat = valhash.eat
  digest_of = valhash.digest_of
  
  if not DONT_REMOVE_TEMPS:
    DONT_REMOVE_TEMPS = os.environ.get('NOBS_DEBUG', False)
  
  @export
  def traced(fn):
    fn._memo_traced = True
    return fn
  
  class MemoizedBase:
    @traced
    def _depend_apathset(me, *a, **kw):
      me._apathset.update(a[-1])
    
  @export
  def memoized(cls):
    cls.__bases__ += (MemoizedBase,)
    uid = digest_of(cls.__module__, cls.__name__, dir(cls), getattr(cls,'unique_id',None))
    cls._uid = uid
    
    def proxy(cxt, *args, **kws):
      return cxt._execute(cls, args, kws)
    
    proxy.__name__ = cls.__name__
    proxy.__doc__ = cls.__doc__
    return proxy
  
  @export
  class MemoDb(object):
    def __init__(db, path_site):
      path_site = os_path_normpath(path_site)
      path_db = os_path_join(path_site, '.nobs', 'db')
      path_art = os_path_join(path_site, '.nobs', 'art')
      
      db.path_site = path_site
      db.path_art = path_art
      
      try: os_makedirs(path_art)
      except OSError: pass
      
      if not os_extra_exists(path_db):
        # db_tree:Tree, Tree = {name_dig: (0, full_dig, call_name, call_args, Tree)
        #                               | (1, full_dig, changed:Promise)
        #                               | (2, full_dig, result_vals, result_kws, artifacts)
        #                               | (3, full_dig, Failure)}
        db._tree = {}
        # db_files: {path: (mtime, digest)}
        db._files = {}
        
        with open(path_db, 'wb') as f:
          cPickle_dump((db._tree, db._files), f, protocol=2)
        db._size_head = 0
        db._size_tail = 0
      else:
        with open(path_db, 'rb') as f:
          db_tree, db_files = cPickle_load(f)
          db._tree = db_tree
          db._files = db_files
          db._size_head = f.tell()
          
          while True:
            try:
              record = cPickle_load(f)
              
              if record[0] == 'tree':
                _, tr_seq, result_vals, result_kws, arts = record
                tip = db_tree
                _, _, name, full = tr_seq[0]
                i_end = len(tr_seq)-1
                i = 0
                while i < i_end:
                  next_call_name, next_call_args, next_name, next_full = tr_seq[i+1]
                  node = tip.get(name)
                  if node is None or node[1] != full:
                    next_tip = {}
                    tip[name] = (0, full, next_call_name, next_call_args, next_tip)
                    tip = next_tip
                  else:
                    tag, _, node_call_name, node_call_args, next_tip = node
                    assert node_call_name == next_call_name and node_call_args == next_call_args
                    tip = next_tip
                  name = next_name
                  full = next_full
                  i += 1
                tip[name] = (2, full, result_vals, result_kws, arts)
                
              elif record[0] == 'file':
                _, apath, mtime, dig = record
                db_files[apath] = (mtime, dig)
                
              else:
                assert False
            
            except Exception:
              db._size_tail = f.tell()
              break
      
      db._lock_acquire = async.CriticalSection(threadsafe=False).acquire
      db._failed_name_seqs = []
      
      db_tree = db._tree
      db_files = db._files
      db_files_get = db_files.get
      db_lock_acquire = db._lock_acquire
      db_failed_name_seqs = db._failed_name_seqs
      
      def artifact_path(pre, dig, suf):
        pre += '.' if pre and not pre.endswith('.') else ''
        suf =  ('.' if suf and not suf.startswith('.') else '')  + suf
        return os_path_join(path_art, pre + hexlify(dig) + suf)
      
      def save():
        @async_mbind(db_lock_acquire())
        def done(release):
          try:
            if 0.33*db._size_head < db._size_tail - db._size_head:
              # prune failures
              for name_seq in db_failed_name_seqs:
                tip = db_tree
                fan_tip = None
                for name in name_seq:
                  if len(tip) > 1:
                    fan_tip = tip
                    fan_name = name
                  
                  node = tip[name]
                  if node[0] == 0:
                    _, _, _, _, tip = node
                  else:
                    assert node[0] == 3 # failed path must end in Failure
                
                if fan_tip is not None:
                  del fan_tip[fan_name]
              
              del db_failed_name_seqs[:]
              
              with open(path_db, 'wb') as f:
                try:
                  cPickle_dump((db_tree, db_files), f, protocol=2)
                except TypeError:
                  #print me._tree
                  raise
                db._size_head = db._size_tail = f.tell()
          finally:
            release()
        
        done.wait()
      # register as method
      db.save = save
      
      def file_digest(apath):
        NO_EXIST = (-1, 'NO_EXIST') # (mtime, digest) of non-existent file
        
        mtime = os_extra.mtime(apath)
        
        if db_files_get(apath, NO_EXIST)[0] != mtime:
          if mtime != -1:
            sha1 = hashlib_sha1()
            up = sha1.update
            up('%x:' % len(apath))
            up(apath)
            with open(apath, 'rb') as f:
              for chk in iter(lambda: f.read(8192), b''):
                up(chk)
            digest = sha1.digest()
            db_files[apath] = (mtime, digest)
          else:
            digest = NO_EXIST[1]
            db_files[apath] = NO_EXIST
          
          record = cPickle_dumps(('file',apath,mtime,digest), protocol=2)
          with open(path_db, 'ab') as f:
            f.write(record)
        
        return db_files_get(apath, NO_EXIST)[1]
      
      def _execute(cls, args, kws):
        return memo_execute(cls, args, kws, set())
      # register as method
      db._execute = _execute
      
      @async_coroutine
      def memo_execute(cls, args, kws, out_apathset):
        tmp_apathset = set()
        
        def make_trace_resultoid_and_digests():
          memo = {}
          memo_get = memo.get
          def trace_resultoid_and_digests(call_name, call_args):
            h = hashlib_sha1()
            call_dig = valhash_eat(h, call_name, call_args).digest()
            
            got = memo_get(call_dig)
            if got is not None:
              return got
            
            cxt_apathset = set()
            cxt = TraceContext(cls, args, kws, cxt_apathset)
            cxt_facts = cxt._facts
            
            @async_after(
              getattr(cls, call_name).im_func, cxt, *(args + call_args), **kws
            )
            def result(result):
              tmp_apathset.update(cxt_apathset)
              
              valhash_eat(h, result, cxt_facts)
              
              h_update = h.update
              h_digest = h.digest
              
              if len(cxt_apathset) != 0:
                apaths = sorted(cxt_apathset)
                
                for apath in apaths:
                  h_update('%x:%s' % (len(apath), apath))
                h_update(';')
                name = h_digest()
                
                for apath in apaths:
                  h_update(file_digest(apath))
                full = h_digest()
              else:
                name = h_digest()
                full = name
              
              return (result.value, name, full)
            
            memo[call_dig] = result
            return result
          return trace_resultoid_and_digests
        
        trace_resultoid_and_digests = make_trace_resultoid_and_digests()
        
        db_lock_release = yield db_lock_acquire()
        
        # run down tree
        tip = db_tree
        name = cls._uid
        full = cls._uid
        trace_seq = [(None, None, name, full)]
        trace_seq_append = trace_seq.append
        call_name, call_args = None, None
        
        while True:
          node = tip.get(name)
          tag = None if node is None else node[0]
          
          if node is None or node[1] != full:
            if node is not None:
              if node[0] == 1:
                raise AssertionError('Same trace and instance generated different full hashes.')
              
              # delete orphaned artifacts
              def prune_arts(node):
                tag = node[0]
                if tag == 0:
                  for name,node1 in node[4].iteritems():
                    prune_arts(node1)
                elif tag == 1:
                  raise AssertionError('Same trace and instance generated different full hashes.')
                elif tag == 2:
                  for art in node[4]:
                    os_extra_rmtree(artifact_path(*art))
              
              prune_arts(node)
            
            tip[name] = (1, full, async_Promise())
            break # execute
            
          elif tag == 1:
            db_lock_release()
            _, _, subtree_changed = node
            yield subtree_changed
            db_lock_release = yield db_lock_acquire()
            
          elif tag in (2,3):
            db_lock_release()
            out_apathset.update(tmp_apathset)
            
            if tag == 2:
              _, _, res_args, res_kws, _ = node
              yield async_Result(*res_args, **res_kws)
            else:
              _, _, failure = node
              yield failure
            return
          
          elif tag == 0:
            db_lock_release()
            
            _, _, call_name, call_args, tip1 = node
            _, name, full = yield trace_resultoid_and_digests(call_name, call_args)
            
            db_lock_release = yield db_lock_acquire()
            tip = tip1
            trace_seq_append((call_name, call_args, name, full))
        
        db_lock_release()
        
        # execute
        cxt = ExecuteContext(cls, args, kws, tip, trace_seq, trace_resultoid_and_digests, tmp_apathset)
        result = async_jailed(getattr(cls, 'execute').im_func, cxt)
        yield async_when_done(result)
        
        out_apathset.update(tmp_apathset)
        
        # finish tree
        db_lock_release = yield db_lock_acquire()
        
        result = result.result()
        cxt_tree_tip = cxt._tree_tip
        cxt_artifacts = cxt._artifacts
        _, _, name, full = trace_seq[-1]
        _, _, subtree_changed = cxt_tree_tip[name]
        
        if not DONT_REMOVE_TEMPS:
          for tmp in cxt._temps:
            os_extra_rmtree(tmp)
        
        if isinstance(result, async_Result):
          cxt_tree_tip[name] = (2, full, result.values(), result.kws(), cxt_artifacts)
          
          # commit record to file
          record = cPickle_dumps(
            ('tree', trace_seq, result.values(), result.kws(), cxt_artifacts),
            protocol=2
          )
          with open(path_db, 'ab', 0) as f:
            f.write(record)
        
        elif isinstance(result, async_Failure):
          cxt_tree_tip[name] = (3, full, result)
          
          if True: # delete artifacts of failed jobs
            for art in cxt_artifacts:
              os_extra_rmtree(artifact_path(*art))
          
          db_failed_name_seqs.append(zip(*trace_seq)[2])
        else:
          assert False
        
        db_lock_release()
        subtree_changed.satisfy()
        
        yield result
      
      class Context(object):
        def __init__(me, cls, tracer, args, kws, apathset):
          me.memodb = db
          me._apathset = apathset
          
          me__dict__ = me.__dict__
          for name in dir(cls):
            if not name.startswith('__'):
              obj = getattr(cls, name)
              if isinstance(obj, types_MethodType):
                obj = obj.im_func
              
              if getattr(obj, '_memo_traced', False):
                me__dict__[name] = tracer(name, obj)
              elif isinstance(obj, types_FunctionType):
                me__dict__[name] = (lambda obj: lambda *a,**kw: obj(me,*a,**kw))(obj)
              else:
                me__dict__[name] = obj
        
        def depend_apathset_smart(me, apathset_more):
          apathset_more.difference_update(me._apathset)
          
          if len(apathset_more) != 0:
            me._depend_apathset(apathset_more)
        
        def depend_files(me, *paths):
          me.depend_apathset_smart(set(map(os_path_abspath, paths)))
      
      class TraceContext(Context):
        def __init__(me, cls, args, kws, apathset):
          assert len(apathset) == 0
          
          def tracer(call_name, call_fn):
            return lambda *a: call_fn(me, *(args + a), **kws)
          
          super(TraceContext, me).__init__(cls, tracer, args, kws, apathset)
          me._facts = {}
          
          def _execute(cls, args, kws):
            return memo_execute(cls, args, kws, out_apathset=apathset)
          # register as method
          me._execute = _execute
        
        def depend_fact(me, key, value):
          me_facts = me._facts
          assert me_facts.get(key, value) == value
          me_facts[key] = value
        
        def depend_facts(me, fact_dict):
          me_facts = me._facts
          for k,v in fact_dict.items():
            assert k not in me_facts or me_facts[k] == v
            me_facts[k] = v
      
      class ExecuteContext(Context):
        def __init__(me, cls, args, kws, tree_tip, trace_seq, trace_resultoid_and_digests, apathset):
          name_map = dict((tr[2],tr) for tr in trace_seq)
          trace_seq_append = trace_seq.append
          
          def tracer(call_name, call_fn):
            def proxy(*call_args):
              @async_after(trace_resultoid_and_digests, call_name, call_args)
              def result(stuff):
                resultoid, name, full = stuff.value()
                if name in name_map:
                  assert name_map[name][3] == full
                else:
                  _, _, name0, full0 = trace_seq[-1]
                  
                  tr = (call_name, call_args, name, full)
                  name_map[name] = tr
                  trace_seq_append(tr)
                  
                  tip = me._tree_tip
                  _, _, changed0 = tip[name0]
                  tree1 = {name: (1, full, async_Promise())}
                  tip[name0] = (0, full0, call_name, call_args, tree1)
                  me._tree_tip = tree1
                  
                  changed0.satisfy()
                return resultoid()
              return result
            return proxy
          
          super(ExecuteContext, me).__init__(cls, tracer, args, kws, apathset)
          me._trace_seq = trace_seq
          me._tree_tip = tree_tip
          me._artifacts = []
          me._temps = []
          
          def _execute(cls, args, kws):
            tmp_apathset = set()
            @async_mbind_jailed(
              memo_execute(cls, args, kws, out_apathset=tmp_apathset)
            )
            def result(result):
              me.depend_apathset_smart(tmp_apathset)
              return result
            return result
          # register as method
          me._execute = _execute
          
        def mkpath(me, key, prefix='', suffix='', isdir=False):
          assert os_path_sep not in prefix
          assert os_path_sep not in suffix
          
          full_seq = sorted(zip(*me._trace_seq)[3])
          dig = valhash_eat(hashlib_sha1(), full_seq, key, prefix, suffix, isdir).digest()
          art = (prefix, dig, suffix)
          me._artifacts.append(art)
          apath = artifact_path(prefix, dig, suffix)
          os_extra_rmtree(apath)
          return apath
          
        def mktree(me, entries, symlinks=True):
          path = me.mkpath(key=entries, isdir=True)
          os_extra.mktree(path, entries, symlinks=symlinks)
          return path
        
        def mktemp(me):
          import tempfile
          fd, path = tempfile.mkstemp()
          os.close(fd)
          os.remove(path)
          me._temps.append(path)
          return path
        
        def mkdtemp(me):
          import tempfile
          path = tempfile.mkdtemp()
          me._temps.append(path)
          return path
        
_everything()
del _everything
