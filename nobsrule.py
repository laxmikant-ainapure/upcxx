"""
This is a nobs rule-file. See nobs/nobs/ruletree.py for documentation
on the structure and interpretation of a rule-file.
"""

import os
import sys

from nobs import subexec

cxx_exts = ('.cpp','.cxx','.c++','.C','.C++')

def env(name, otherwise):
  """
  Read `name` from the environment returning it as an integer if it
  looks like one otherwise a string. If `name` does not exist in the 
  environment then `otherwise` is returned.
  """
  try:
    got = os.environ[name]
    try: return int(got)
    except ValueError: pass
    return got
  except KeyError:
    return otherwise

"""
Library sets are encoded as a dictionary of the type:
  {libname:str: {
      'ld':[str],
      'ppflags':[str],
      'cgflags':[str],
      'ldflags':[str],
      'libflags':[str],
      'deplibs':[str]
    }, ...
  }.

That is, each key is the short name of the library (like 'm') and the 
value is a dictionary containing the linker command, various flags
lists, and the list of libraries short-names it is dependent on. If
'libflags' is absent it defaults to ['-l'+libname].
"""

def libset_merge_inplace(dst, src):
  for k,v in src.items():
    if dst.get(k,v) != v:
      raise Exception("Multiple '%s' libraries with differing configurations." % k)
    dst[k] = v

def libset_ld(libset):
  lds = set(tuple(x.get('ld',())) for x in libset.values())
  lds.discard(())
  if len(lds) == 0:
    return None
  if len(lds) != 1:
    raise Exception("Multiple linkers demanded:" + ''.join(map(lambda x:'\n  '+' '.join(x), lds)))
  return list(lds.pop())

def libset_flags(libset, kind):
  flags = []
  for x in libset.values():
    flags.extend(x.get(kind+'flags', []))
  return flags

@cached
def output_of(cmd_args):
  """
  Returns (returncode,stdout,stderr) generated by invoking the command
  arguments as a child process.
  """
  import subprocess as sp
  p = sp.Popen(cmd_args, stdin=sp.PIPE, stdout=sp.PIPE, stderr=sp.PIPE)
  stdout, stderr = p.communicate()
  return (p.returncode, stdout, stderr)

@rule(cli='cxx')
def cxx(cxt):
  """
  String list for the C++ compiler. Defaults to g++.
  """
  return env('CXX', otherwise='g++').split()

@rule(cli='cc11')
def cc(cxt):
  """
  String list for the C compiler. Defaults to gcc.
  """
  return env('CC', otherwise='gcc').split()

@rule()
def lang_c11(cxt):
  """
  String list to engage C11 language dialect for the C compiler.
  """
  return ['-std=c11']

@rule()
def lang_cxx11(cxt):
  """
  String list to engage C++11 language dialect for the C++ compiler.
  """
  return ['-std=c++11']

def sanitized_output_of(cmd):
  """
  Not using this yet since it's very non-portable. Intended to strip
  paths to temporary things from the output generated by "gcc -v".
  """
  import re
  rc,out,err = output_of(cmd)
  sanitize = lambda s: re.sub(r'/tmp/[^/]*\.[^/.]+', '', s)
  return (rc, sanitize(out), sanitize(err))

@rule()
def cxx_version(cxt):
  """
  Value representing the C++ compiler identity.
  """
  cxx = cxt.cxx()
  return output_of(cxx + ['--version'])

@rule()
def cc_version(cxt):
  """
  Value representing the C compiler identity.
  """
  cc = cxt.cc()
  return output_of(cc + ['--version'])

@rule(path_arg='src')
@coroutine
def cxx11_pp(cxt, src):
  """
  File-specific C++11 compiler with preprocessor flags.
  """
  cxx11 = cxt.cxx() + cxt.lang_cxx11()
  ipt = yield cxt.include_paths_tree()
  libs = yield cxt.libraries(src)
  yield cxx11 + ['-I'+ipt] + libset_flags(libs, 'pp')

@rule()
def cg_optlev(cxt):
  """
  The default code-gen optimization level for compilation. Reads the
  "OPTLEV" environment variable.
  """
  return env('OPTLEV', 2)

@rule(path_arg='src')
def cg_optlev_forfile(cxt, src):
  """
  File-specific code-gen optimization level, defaults to `cg_optlev`.
  """
  return cxt.cg_optlev()

@rule()
def cg_dbgsym(cxt):
  """
  Include debugging symbols and instrumentation in compiled files.
  """
  return env('DBGSYM', 0)

@rule(path_arg='src')
@coroutine
def cxx11_pp_cg(cxt, src):
  """
  File-specific C++11 compiler with preprocessor and code-gen flags.
  """
  cxx11_pp = yield cxt.cxx11_pp(src)
  optlev = cxt.cg_optlev_forfile(src)
  dbgsym = cxt.cg_dbgsym()
  libset = yield cxt.libraries(src)
  
  yield (
    cxx11_pp +
    ['-O%d'%optlev] +
    (['-g'] if dbgsym else []) +
    ['-Wall'] +
    libset_flags(libset, 'cg')
  )

@rule(path_arg='src')
@coroutine
def compiler(cxt, src):
  """
  File-specific compiler lambda. Given a source file path, returns a
  function that given a path of where to place the object file, returns
  the argument list to invoke as a child process.
  """
  cxx11_pp_cg = yield cxt.cxx11_pp_cg(src)
  yield lambda outfile: cxx11_pp_cg + ['-c', src, '-o', outfile]

@rule(path_arg='src')
@coroutine
def libraries(cxt, src):
  """
  File-specific library set required to compile and eventually link the
  file `src`.
  """
  if src == here('test','gasnet_hello.cpp'):
    yield cxt.libgasnet()
  else:
    yield {}

@rule()
def gasnet_conduit(cxt):
  """
  GASNet conduit to use.
  """
  return env('GASNET_CONDUIT','smp')

@rule()
def gasnet_syncmode(cxt):
  """
  GASNet sync-mode to use.
  """
  # this should be computed based off the choice of upcxx backend
  return 'seq'

@rule_memoized()
class include_paths_tree:
  """
  Setup a shim directory containing a single symlink named 'upcxx' which
  points to 'upcxx/src'. With this directory added via '-I...' to
  compiler flags, allows our headers to be accessed via:
    #include <upcxx/*.hpp>
  """
  def execute(cxt):
    return cxt.mktree({'upcxx': here('src')}, symlinks=True)

@rule_memoized(cli='incs', path_arg=0)
class includes:
  """
  Ask compiler for all the non-system headers pulled in by preprocessing
  the given source file. Returns the list of header paths.
  """
  @traced
  @coroutine
  def get_cxx11_pp_and_src(me, cxt, src):
    cxx11_pp = yield cxt.cxx11_pp(src)
    me.depend_files(src)
    me.depend_fact(key=None, value=cxt.cxx_version())
    yield cxx11_pp, src
  
  @coroutine
  def execute(me):
    cxx11_pp, src = yield me.get_cxx11_pp_and_src()
    cmd = cxx11_pp + ['-MM','-MT','x',src]
    
    mk = yield subexec.launch(cmd, capture_stdout=True)
    mk = mk[mk.index(":")+1:]
    
    import shlex
    deps = shlex.split(mk.replace("\\\n",""))[1:] # first is source file
    deps = map(os.path.abspath, deps)
    me.depend_files(*deps)    
    deps = map(os.path.realpath, deps)
    
    yield deps

@rule_memoized(cli='obj', path_arg=0)
class compile:
  """
  Compile the given source file. Returns path to object file.
  """
  @traced
  @coroutine
  def get_src_compiler(me, cxt, src):
    compiler = yield cxt.compiler(src)
    me.depend_fact(key='CXX', value=cxt.cxx_version())
    
    includes = yield cxt.includes(src)
    me.depend_files(src)
    me.depend_files(*includes)
    
    yield src, compiler
  
  @coroutine
  def execute(me):
    src, compiler = yield me.get_src_compiler()
    
    objfile = me.mkpath(None, suffix='.o')
    yield subexec.launch(compiler(objfile))
    yield objfile

@rule_memoized(cli='exe', path_arg=0)
class executable:
  """
  Compile the given source file as well as all source files which can
  be found as sharing its name with a header included by any source
  file reached in this process (transitively closed set). Take all those
  compiled object files and link them along with their library
  dependencies to proudce an executable. Path to executable returned.
  """
  @traced
  def main_src(me, cxt, main_src):
    return main_src
  
  @traced
  def cxx(me, cxt, main_src):
    return cxt.cxx()
  
  @traced
  def do_includes(me, cxt, main_src, src):
    return cxt.includes(src)
  
  @traced
  def do_compile_and_libraries(me, cxt, main_src, src):
    return futurize(cxt.compile(src), cxt.libraries(src))
  
  @traced
  def find_src_exts(me, cxt, main_src, base):
    def exists(ext):
      path = base + ext
      me.depend_files(path)
      return os.path.exists(path)
    return filter(exists, ('.c',) + cxx_exts)
  
  @coroutine
  def execute(me):
    main_src = me.main_src()
    
    # compile object files
    incs_seen = set()
    objs = []
    libset = {}
    
    def fresh_src(src):
      return async.when_succeeded(
        me.do_includes(src) >> includes_done,
        me.do_compile_and_libraries(src) >> compile_done
      )
    
    os_path_splitext = os.path.splitext
    os_path_relpath = os.path.relpath
    src_dir = here('src')
    dot_dot_slash = '..' + os.path.sep
    
    def includes_done(incs):
      tasks = []
      for inc in incs:
        inc, _ = os_path_splitext(inc)
        if inc not in incs_seen:
          incs_seen.add(inc)
          if not os_path_relpath(inc, src_dir).startswith(dot_dot_slash):
            for ext in me.find_src_exts(inc):
              tasks.append(fresh_src(inc + ext))
      
      return async.when_succeeded(*tasks)
    
    def compile_done(obj, more_libs):
      objs.append(obj)
      libset_merge_inplace(libset, more_libs)
    
    # wait for all compilations
    yield fresh_src(main_src)
    
    # topsort library flags by library-library dependencies
    sorted_libflags = []
    sorted_libs = set()
    def topsort(xs):
      for x in xs:
        rec = libset.get(x, {})
        libflags = rec.get('libflags', ['-l'+x])
        deplibs = rec.get('deplibs', [])
        
        topsort(deplibs)
        
        if x not in sorted_libs:
          sorted_libs.add(x)
          sorted_libflags.append(libflags)
    
    topsort(libset)
    sorted_libflags = sum(reversed(sorted_libflags), [])
    
    # link
    exe = me.mkpath('exe', suffix='.x')
    
    ld = libset_ld(libset)
    cxx = me.cxx()
    if ld is None:
      ld = cxx
    ld = [cxx[0]] + ld[1:] 
    
    ldflags = libset_flags(libset, 'ld')
    
    yield subexec.launch(
      ld + ldflags + ['-o',exe] + objs + sorted_libflags
    )
    
    yield exe

@rule_memoized(cli='download')
class download:
  """
  Download a file from url. Returns path to local file.
  """
  @traced
  def get_url(me, cxt, url):
    return url
  
  @coroutine
  def execute(me):
    url = me.get_url()
    dest = me.mkpath(key=url)
    
    @async.launched
    def retrieve():
      import urllib
      urllib.urlretrieve(url, dest)
    
    print>>sys.stderr, 'Downloading %s' % url 
    yield retrieve()
    print>>sys.stderr, 'Finished    %s' % url
    
    yield dest

@rule_memoized()
class libgasnet_source:
  """
  Download and extract gasnet source tree.
  """
  @coroutine
  def execute(me):
    import base64
    gasnetex_tgz_url = base64.b64decode('aHR0cDovL2dhc25ldC5sYmwuZ292L0VYL0dBU05ldC0yMDE3LjYuMC50YXIuZ3o=')
    
    tgz = me.mktemp()
    
    @async.launched
    def download():
      import urllib
      urllib.urlretrieve(gasnetex_tgz_url, tgz)
    
    print>>sys.stderr, 'Downloading %s' % gasnetex_tgz_url 
    yield download()
    print>>sys.stderr, 'Finished    %s' % gasnetex_tgz_url
    
    untar_dir = me.mkpath(key=None)
    os.makedirs(untar_dir)
    
    import tarfile
    with tarfile.open(tgz) as f:
      source_dir = os.path.join(untar_dir, f.members[0].name)
      f.extractall(untar_dir)
    
    yield source_dir

@rule_memoized()
class libgasnet_configured:
  """
  Configure gasnet build directory.
  """
  @traced
  def get_config(me, cxt):
    me.depend_fact(key='CC', value=cxt.cc_version())
    me.depend_fact(key='CXX', value=cxt.cxx_version())
    return (
      cxt.cc() + ['-O%d'%cxt.cg_optlev()],
      cxt.cxx() + ['-O%d'%cxt.cg_optlev()],
      cxt.cg_dbgsym()
    )
  
  @traced
  def get_soruce_dir(me, cxt):
    return cxt.libgasnet_source()
  
  @coroutine
  def execute(me):
    cc, cxx, debug = me.get_config()
    source_dir = yield me.get_soruce_dir()
    
    build_dir = me.mkpath(key=None)
    os.makedirs(build_dir)
    
    env1 = dict(os.environ)
    env1['CC'] = ' '.join(cc)
    env1['CXX'] = ' '.join(cxx)
    
    print>>sys.stderr, 'Configuring GASNet...'
    yield subexec.launch(
      [os.path.join(source_dir, 'configure')] +
      (['--enable-debug'] if debug else []),
      cwd = build_dir,
      env = env1
    )
    
    yield build_dir

@rule_memoized(cli='libgasnet')
class libgasnet:
  """
  Build gasnet. Return library dependencies dictionary.
  """
  @traced
  def get_config(me, cxt):
    return futurize(
      cxt.gasnet_conduit(),
      cxt.gasnet_syncmode(),
      cxt.libgasnet_configured()
    )
  
  @coroutine
  def execute(me):
    conduit, syncmode, build_dir = yield me.get_config()
    
    print>>sys.stderr, 'Building GASNet (conduit=%s, threading=%s)...'%(conduit, syncmode)
    yield subexec.launch(
      ['make', syncmode],
      cwd = os.path.join(build_dir, '%s-conduit'%conduit)
    )
    
    makefile = os.path.join(
      build_dir,
      '%s-conduit'%conduit,
      '%s-%s.mak'%(conduit, syncmode)
    )
    
    GASNET_LD = makefile_extract(makefile, 'GASNET_LD').split()
    GASNET_LDFLAGS = makefile_extract(makefile, 'GASNET_LDFLAGS').split()
    GASNET_CXXCPPFLAGS = makefile_extract(makefile, 'GASNET_CXXCPPFLAGS').split()
    GASNET_CXXFLAGS = makefile_extract(makefile, 'GASNET_CXXFLAGS').split()
    GASNET_LIBS = makefile_extract(makefile, 'GASNET_LIBS').split()
    
    yield {
      'gasnet': {
        'ld': GASNET_LD,
        'ldflags': GASNET_LDFLAGS,
        'ppflags': GASNET_CXXCPPFLAGS,
        'cgflags': GASNET_CXXFLAGS,
        'libflags': GASNET_LIBS,
        'deplibs': []
      }
    }

def makefile_extract(makefile, varname):
  """
  Extract a variable's value from a makefile.
  """
  import subprocess as sp
  p = sp.Popen(['make','-f','-','gimme'], stdin=sp.PIPE, stdout=sp.PIPE, stderr=sp.PIPE)
  tmp = ('include {0}\n' + 'gimme:\n' + '\t@echo $({1})\n').format(makefile, varname)
  val, _ = p.communicate(tmp)
  if p.returncode != 0:
    raise Exception('Makefile %s not found.'%makefile)
  val = val.strip(' \t\n')
  return val

@rule(cli='run', path_arg='main_src')
@coroutine
def run(cxt, main_src, *args):
  """
  Build the executable for `main_src` and run it with the given
  argument list `args`.
  """
  exe = yield cxt.executable(main_src)
  os.execvp(exe, [exe] + map(str, args))
