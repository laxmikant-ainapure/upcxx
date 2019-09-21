"""
This is a nobs rule-file. See nobs/nobs/ruletree.py for documentation
on the structure and interpretation of a rule-file.
"""

import os
import sys

import shlex
shplit = shlex.split

from nobs import errorlog
from nobs import os_extra
from nobs import subexec

# DOB: Do not use '.C' below because the ambiguity with '.c' breaks nobs on case-insensitive filesystems (NTFS)
cxx_exts = ('.cpp','.cxx','.c++','.cc','.C++')
c_exts = ('.c',)
header_exts = ('.h','.hpp','.hxx','.h++','.hh')

crawlable_dirs = [
  here('src'),
  here('test'),
  here('bench')
]

"""
Library sets are encoded as a dictionary of strings to dictionaries
conforming to:
  {libname:str: {
      'ld':[str],
      'incdirs':[str], # "-I" directories
      'incfiles':[str] # abs-paths to files residing in "-I" directories.
      'ppdefs':{str:str/int} # preprocessor macro definitions
      'ppflags':[str], # preprocessor flags to compiler minus those necessary for `incdirs` and `incfiles` and `ppdefs`
      'cgflags':[str], # code-gen flags to compiler
      'ldflags':[str], # flags to ld
      'libfiles':[str], # paths to binary archives
      'libflags':[str], # "-L", "-l", and other trailing linker flags minus those for `libfiles`
      'libdeps':[str] # short-names for dependencies of this library during linking
      'ppdeps': [str] # short-names for dependencies of this library during preprocessing
    }, ...
  }

Each key is the short name of the library (like 'm' for 
'libm') and the value is a dictionary containing the linker command, 
various flags lists, and the list of libraries short-names it is 
dependent on.

`incdirs`, `incfiles` and `libfiles` are not included in the `ppflags`
and `libflags` flag lists. `libset_*flags` queries exist at the bottom of
this file for properly retrieving complete flag lists.

Other routines for manipulating library-sets (named libset_*) exist at
the bottom of this file as well.

Libraries here need not be actual "libfoo.a" style archives, pseudo
libraries which just act as a collection of preprocessor flags are
valid too.
"""

########################################################################

def upcxx_backend_id():
  return env("UPCXX_BACKEND",
             otherwise="gasnet_seq",
             universe=("gasnet_seq","gasnet_par"))

def upcxx_make():
  return shplit(env("UPCXX_MAKE", "make -j 8"))

@rule(cli='pthread')
@coroutine
def pthread(cxt):
  """
  Return the library-set for building against pthreads. 
  """
  cxx = yield cxt.cxx()
  if is_pgi(cxx):
    flag = '-lpthread'
  else:
    flag = '-pthread'
  yield {'pthread':{'libflags':[flag]}}

@rule(cli='omp')
@coroutine
def openmp(cxt):
  cxx = yield cxt.cxx()
  if is_pgi(cxx):
    flags = ['-mp']
  else:
    flags = ['-fopenmp']
  
  yield {'openmp': {
    'ppflags': flags,
    'libflags': flags
  }}

def invoke(cmd, shell=False):
  import subprocess as subp
  try:
    # Need to use shell=True so that commands parsed from nvcc are executed as
    # if in a shell.
    p = subp.Popen(' '.join(cmd), stdin=subp.PIPE, stdout=subp.PIPE, stderr=subp.PIPE, close_fds=True, shell=shell)
  except OSError as e:
    return (e.errno, '', '')
  out,err = p.communicate()
  ret = p.returncode
  return (ret,out,err)

def invoke_on_file(suffix, contents, file_to_cmd, file_to_outs=lambda x:[]):
  import os
  import tempfile
  fd,name = tempfile.mkstemp(suffix=suffix)
  os.write(fd, contents)
  os.close(fd)
  ans = invoke(file_to_cmd(name), shell=True)
  for f in [name] + list(file_to_outs(name)):
    try: os.remove(f)
    except OSError: pass
  return ans

@rule(cli='cuda')
def cuda(cxt):
  return cuda_cached()

@cached
def cuda_cached():
  """
  Platform specific logic for finding cuda goes here.
  """
  import re

  nvcc = env('UPCXX_CUDA_NVCC','nvcc').split()
  
  ##############################################################################
  # scrape the preprocessor flags and backend compiler
  res,out,err = invoke(nvcc+['--dryrun','-c','foo.cpp','-o','foo.o'], shell=True)
  if res != 0:
    raise errorlog.LoggedError(
      'nvcc error',
      'Failed to invoke nvcc (name=%s, exit=%d):\n'%(' '.join(nvcc), res) +
      '\n' +
      'Make sure CUDA is installed and either nvcc is in your PATH,\n' +
      'or set UPCXX_CUDA_NVCC in your environment to point to it.'
    )
  lines = [line for line in err.split('\n') if line != '']
  ppflags = []
  
  for line in lines:
    m1 = re.match(r'^#\$ INCLUDES=(.*)$', line)
    m2 = re.match(r'^#\$ ([A-Za-z0-9_+-/."]+) ', line)
    if m1:
      flags = m1.group(1).split()
      flags = [flag[1:-1] if flag.startswith('"') else flag for flag in flags]
      ppflags += [flag for flag in flags if re.match('^(-D|-I)', flag)]
    elif m2:
      compiler_bindir = os.path.dirname(m2.group(1))
      compiler_binary = {'gcc':'g++', 'clang':'clang++', 'icc':'icpc', 'pgcc':'pgc++'}[os.path.basename(m2.group(1))]
      compiler = os.path.join(compiler_bindir, compiler_binary)

  if env('UPCXX_CUDA_CPPFLAGS', None) is not None:
    # override scraped ppflags
    ppflags = env('UPCXX_CUDA_CPPFLAGS',None).split()
  
  ##############################################################################
  libflags = env('UPCXX_CUDA_LIBFLAGS', None)
  if libflags is not None:
    libflags = libflags.split()
  else:
    # scrape the link flags
    res,out,err = invoke(nvcc+['--dryrun','foo.o'], shell=True)
    if res != 0:
      raise errorlog.LoggedError('nvcc error', 'Failed to invoke nvcc (exit=%d).\nMake sure CUDA is installed and nvcc is in your PATH.'%res)
    line = [line for line in err.split('\n') if line != ''][-1] # last line
    m1 = re.match(r'^#\$ ([A-Z_]+)=', line)
    m2 = re.match(r'^#\$ ([A-Za-z0-9_+-/."]+) ', line)
    libflags = []
    
    if not(m1 is None and m2):
      raise errorlog.LoggedError('nvcc error', '"nvcc --dryrun" output format unrecognized.\n' + line)
    else:
      flags = [flag[1:-1] if flag.startswith('"') else flag for flag in line.split()]
      flags = [flag for flag in flags if not flag.endswith('.o')]
      flags.reverse()
      while flags:
        if flags[-1] == '-Wl,--start-group':
          end = flags.index('-Wl,--end-group')
          libflags += reversed(flags[end:])
          del flags[end:]
        elif re.match('^(-L|-l|-Wl,)', flags[-1]):
          libflags.append(flags[-1])
          del flags[-1]
        elif flags[-1] in ('-Xlinker','-rpath'):
          libflags += reversed(flags[-2:])
          del flags[-2:]
        elif flags[-1] in ('-pthread',):
          libflags += flags[-1]
          del flags[-1]
        else:
          del flags[-1]

  ##############################################################################
  # check that the CUDA Driver API is linkable
  res,out,err = invoke_on_file(
    '.cpp',
    '#include <cuda.h>\n' +
    'int main() { cuInit(0); return 0; }\n',
    lambda cpp: [compiler] + ppflags + [cpp,'-o',cpp+'.exe'] + libflags,
    lambda cpp: [cpp+'.exe']
  )
  if res != 0: # link failed, try adding -lcuda and others
    for extra in [['-lcuda'], ['-framework','CUDA']]:
      res,out,err = invoke_on_file(
        '.cpp',
        '#include <cuda.h>\n' +
        'int main() { cuInit(0); return 0; }\n',
        lambda cpp: [compiler] + ppflags + [cpp,'-o',cpp+'.exe'] + libflags + extra,
        lambda cpp: [cpp+'.exe']
      )
      if res == 0:
        libflags += extra
        break

    if res != 0:
      raise errorlog.LoggedError(
        'nvcc error',
        'Failed to link against CUDA Driver API:\n' +
        '\n' +
        'foo.cpp:\n' +
        '  #include <cuda.h>\n' +
        '  int main() { cuInit(0); return 0; }\n' +
        '\n' +
        'command:\n' +
        '  '+' '.join([compiler]+ppflags+['foo.cpp','-o','foo.exe']+libflags)
      )
  
  return {'cuda': {'ppflags': ppflags, 'libflags': libflags}}

# Rule overriden in sub-nobsrule files.
@rule(cli='requires_gasnet', path_arg='src')
def requires_gasnet(cxt, src):
  return False

# Rule overriden in sub-nobsrule files.
@rule(cli='requires_pthread', path_arg='src')
def requires_pthread(cxt, src):
  return False

# Rule overriden in sub-nobsrule files.
@rule(cli='requires_openmp', path_arg='src')
def requires_openmp(cxt, src):
  return False

@rule(cli='q', path_arg='src')
@coroutine
def required_libraries(cxt, src):
  """
  File-specific library set required to compile and eventually link the
  file `src`.
  """
  if cxt.requires_gasnet(src):
    maybe_gasnet = yield cxt.gasnet()
  else:
    maybe_gasnet = {}
  
  if cxt.requires_pthread(src):
    maybe_pthread = yield cxt.pthread()
  else:
    maybe_pthread = {}

  if cxt.requires_openmp(src):
    maybe_openmp = yield cxt.openmp()
  else:
    maybe_openmp = {}

  yield libset_merge(maybe_gasnet, maybe_pthread, maybe_openmp)

@rule()
def gasnet_user(cxt):
  value = env('GASNET', None)
  
  if not value:
    value = 'https://gasnet-bugs.lbl.gov/nightly/unlisted/GASNet-stable.tar.gz'
  
  from urlparse import urlparse
  isurl = urlparse(value).netloc != ''
  if isurl:
    return 'tarball-url', value
  
  if not os_extra.exists(value):
    raise errorlog.LoggedError("Non-existent path for GASNET="+value)
  
  value = os.path.abspath(value)
  join = os.path.join
  
  if os_extra.isfile(value):
    return 'tarball', value
  elif os_extra.exists(join(value, 'Makefile')):
    return 'build', value
  elif os_extra.exists(join(value, 'include')) and \
       os_extra.exists(join(value, 'lib')):
    return 'install', value
  elif os_extra.exists(join(value, 'configure')):
    return 'source', value
  else:
    raise errorlog.LoggedError("Invalid value for GASNET (GASNET=%s)"%value)

@rule(cli='gasnet_user_kind')
def gasnet_user_kind(cxt):
  kind, _ = cxt.gasnet_user()
  return kind

@rule(cli='gasnet_conduit')
def gasnet_conduit(cxt):
  """
  GASNet conduit to use.
  """
  if env('CROSS','').startswith('cray-aries-'):
    default = 'aries'
  else:
    default = 'smp'
  
  return env('GASNET_CONDUIT', None) or default

@rule(cli='gasnet_syncmode')
def gasnet_syncmode(cxt):
  """
  GASNet sync-mode to use.
  """
  return {
      'gasnet_seq': 'seq',
      'gasnet_par': 'par'
    }[upcxx_backend_id()]

@rule(cli='gasnet_syncmode')
def gasnet_debug(cxt):
  """
  Whether to build GASNet in debug mode.
  """
  return cxt.cg_dbgsym() and cxt.cg_optlev_default() == 0

@rule(cli='gasnet_install_to')
def gasnet_install_to(cxt):
  """
  User-requested install location for gasnet.
  """
  path_fmt = env('GASNET_INSTALL_TO', None)
  
  if path_fmt is None:
    return None
  else:
    path = path_fmt.format(debug=(1 if cxt.gasnet_debug() else 0))
    path = os.path.expanduser(path)
    path = os.path.abspath(path)
    return path

########################################################################
## Compiler toolcahin etc.
########################################################################

@rule(cli='cxx')
@coroutine
def cxx(cxt):
  """
  String list for the C++ compiler.
  """
  _, cross_env = yield cxt.gasnet_config()
  ans_cross = shplit(cross_env.get('CXX',''))
  
  ans_default = []
  if env('CRAYPE_DIR', None):
    ans_default = ['CC']
  if not ans_default:
    ans_default = ['g++']
  
  ans_user = shplit(env('CXX',''))
  
  if ans_cross and ans_user and ans_user != ans_cross:
    errorlog.warning(
      "Cross C++ compiler (%s) differs from CXX environment variable (%s)." % (
        ' '.join(ans_cross),
        ' '.join(ans_user)
      )
    )
  
  # If the cross-config script set it, use it.
  # Otherwise honor the CXX env-variable.
  # Otherwise use intelligent defaults.
  yield ans_cross or ans_user or ans_default

@rule(cli='cc')
@coroutine
def cc(cxt):
  """
  String list for the C compiler.
  """
  _, cross_env = yield cxt.gasnet_config()
  ans_cross = shplit(cross_env.get('CC',''))
  
  ans_default = []
  if env('CRAYPE_DIR', None):
    ans_default = ['cc']
  if not ans_default:
    ans_default = ['gcc']
  
  ans_user = shplit(env('CC',''))
  
  if ans_cross and ans_user and ans_user != ans_cross:
    errorlog.warning(
      "Cross C compiler (%s) differs from CC environment variable (%s)." % (
        ' '.join(ans_cross),
        ' '.join(ans_user)
      )
    )
  
  # If the cross-config script set it, use it.
  # Otherwise honor the CC env-variable.
  # Otherwise use intelligent defaults.
  yield ans_cross or ans_user or ans_default

def is_pgi(cmd):
  _,out,_ = version_of(cmd)
  import re
  return re.search('PGI Compilers and Tools', out) is not None

@rule(cli='ldflags')
def ldflags(cxt):
  return shplit(env('LDFLAGS',''))

@rule()
@coroutine
def lang_cxx11(cxt):
  """
  String list to engage C++11 language dialect for the C++ compiler.
  """
  cxx = yield cxt.cxx()
  _,out,_ = yield version_of(cxx)

  if any(arg.startswith('-std=c++') for arg in cxx):
    yield []
  elif any(token in out for token in ['(ICC)']):
  #elif any(token in out for token in ['PGI Compilers and Tools','(ICC)']):
    yield ['-std=c++14']
  else:
    yield ['-std=c++11']

@rule(path_arg='src')
@coroutine
def comp_lang(cxt, src):
  """
  File-specific compiler with source-language dialect flags.
  """
  _, ext = os.path.splitext(src)
  
  if ext in cxx_exts:
    cxx = yield cxt.cxx()
    cxx11 = yield cxt.lang_cxx11()
    yield cxx + cxx11
  elif ext in c_exts:
    cc = yield cxt.cc()
    yield cc
  else:
    raise Exception("Unrecognized source file extension: "+src)

def version_of(cmd):
  return output_of(cmd + ['--version'])

@rule(path_arg='src')
@coroutine
def comp_version(cxt, src):
  """
  Identity string of file-specific compiler.
  """
  _, ext = os.path.splitext(src)
  
  if ext in cxx_exts:
    cxx = yield cxt.cxx()
    yield version_of(cxx)
  elif ext in c_exts:
    cc = yield cxt.cc()
    yield version_of(cc)
  else:
    raise Exception("Unrecognized source file extension: "+src)

@rule()
def upcxx_assert_enabled(cxt):
  return bool(env('ASSERT', False))

@rule()
def upcxx_cuda_enabled(cxt):
  return bool(env('UPCXX_CUDA', False))

@rule(path_arg='src')
@coroutine
def comp_lang_pp(cxt, src, libset):
  """
  File-specific compiler with source-language and preprocessor flags.
  """
  comp = yield cxt.comp_lang(src)
  ivt = yield cxt.include_vdirs_tree(src)
  yield (
    comp + 
    ['-D_GNU_SOURCE=1'] + # Required for full latest POSIX on some systems
    ['-I'+ivt] +
    libset_ppflags(libset)
  )

@rule()
def cg_optlev_default(cxt):
  """
  The default code-gen optimization level for compilation. Reads the
  "OPTLEV" environment variable.
  """
  return env('OPTLEV', 2)

@rule(cli='cg_optlev', path_arg='src')
def cg_optlev(cxt, src):
  """
  File-specific code-gen optimization level, defaults to `cg_optlev_default`.
  """
  return cxt.cg_optlev_default()

@rule()
def cg_dbgsym(cxt):
  """
  Include debugging symbols.
  """
  return env('DBGSYM', 0)

@rule(cli='comp_lang_pp_cg', path_arg='src')
@coroutine
def comp_lang_pp_cg(cxt, src, libset):
  """
  File-specific compiler with language, preprocessor, and code-gen flags.
  """
  optlev = cxt.cg_optlev(src)
  dbgsym = cxt.cg_dbgsym()
  comp = yield cxt.comp_lang_pp(src, libset)
  
  yield (
    comp +
    ['-O%d'%optlev] +
    (['-g'] if dbgsym else []) +
    (['-Wall'] if not is_pgi(comp) else []) +
    libset_cgflags(libset)
  )

@rule(path_arg='src')
@coroutine
def compiler(cxt, src, libset):
  """
  File-specific compiler lambda. Given a source file path, returns a
  function that given a path of where to place the object file, returns
  the argument list to invoke as a child process.
  """
  comp = yield cxt.comp_lang_pp_cg(src, libset)
  
  yield lambda outfile: comp + ['-c', src, '-o', outfile]


########################################################################
## Compiler invocation
########################################################################

@rule(cli='include_vdirs', path_arg='src')
def include_vdirs(cxt, src):
  return {'upcxx': here('src')}

@rule_memoized(path_arg=0)
class include_vdirs_tree:
  """
  Setup a shim directory containing a symlink for each vdir in
  include_vdirs. With this directory added via '-I...' to
  compiler flags, allows our headers to be accessed via:
    #include <upcxx/*.hpp>
  """
  version_bump = 1
  
  @traced
  def include_vdirs(me, cxt, src):
    return cxt.include_vdirs(src)
  
  def execute(me):
    vdirs = me.include_vdirs()
    return me.mktree(vdirs, symlinks=True)

@rule_memoized(path_arg=0)
class includes:
  """
  Ask compiler for all the non-system headers pulled in by preprocessing
  the given source file. Returns the list of paths to included files.
  """
  version_bump = 7
  
  @traced
  @coroutine
  def src_and_compiler(me, cxt, src, global_libset):
    me.depend_files(src)
    
    version = yield cxt.comp_version(src)
    me.depend_fact(key=None, value=version)

    comp_pp = yield cxt.comp_lang_pp(src, global_libset)
    
    yield (src, comp_pp)

  @coroutine
  def execute(me):
    src, comp_pp = yield me.src_and_compiler()
    
    # See here for getting this to work with other compilers:
    #  https://projects.coin-or.org/ADOL-C/browser/trunk/autoconf/depcomp?rev=357
    cmd = comp_pp + ['-DNOBS_DISCOVERY','-M','-MT','x']
    
    if is_pgi(comp_pp):
      cmd += ['-E']
    cmd += [src]
    
    mk = yield subexec.launch(cmd, capture_stdout=True)
    mk = mk[mk.index(":")+1:]
    
    deps = shplit(mk.replace("\\\n",""))[1:] # first is source file
    deps = map(os.path.abspath, deps)

    # prune files not in the upcxx tree (would be accomplished by -MM in compiler
    # command above except PGI doesn't implement that correctly)
    top_dir = here()
    deps = [d for d in deps if
      path_within_dir(d, top_dir) or
      path_within_dir(d, me.memodb.path_art)
    ]
    
    me.depend_files(*deps)
    deps = map(os_extra.realpath, deps)
    deps = sorted(set(deps))

    #print>>sys.stderr, 'INC DEPS:',deps
    yield deps

@rule_memoized(path_arg=0)
class compile:
  """
  Compile the given source file. Returns path to object file.
  """
  version_bump = 1
  
  @traced
  @coroutine
  def src_and_compiler(me, cxt, src, pp_libset):
    compiler = yield cxt.compiler(src, pp_libset)
    version = yield cxt.comp_version(src)
    
    me.depend_fact(key='compiler', value=version)
    
    includes = yield cxt.includes(src, pp_libset)
    me.depend_files(src)
    me.depend_files(*includes)
    
    yield (src, compiler)
  
  @coroutine
  def execute(me):
    src, compiler = yield me.src_and_compiler()
    
    objfile = me.mkpath(None, suffix=os.path.basename(src)+'.o')
    yield subexec.launch(compiler(objfile))
    yield objfile

@coroutine
def _includes_and_libset(cxt, src, libset0):
  """
  Given source file `src` and an initial library set `libset0`, returns
  set of included files and the larger libset consisting of the
  accumulation of: `libset0`, and `required_libraries()` for `src` and
  all of its includes.
  """
  src_libset = yield cxt.required_libraries(src)
  
  incs = yield cxt.includes(src, libset_merge(libset0, src_libset))
  
  incs = filter( # `inc` must be in a crawlable directory
    lambda inc: any(path_within_dir(inc, x) for x in crawlable_dirs),
    incs
  )
  
  libset1 = dict(src_libset)
  for inc in incs:
    inc_libset = yield cxt.required_libraries(inc)
    libset_merge_inplace(libset1, inc_libset)
  
  yield (incs, libset1)

def _find_src_exts(base, depend_files):
  def exists(ext):
    path = base + ext
    depend_files(path)
    return os_extra.exists(path)
  srcs = filter(exists, c_exts + cxx_exts)
  return srcs

@coroutine
def _source_discovery(main_src, includes_and_libset, find_src_exts):
  """
  Returns the (sources:set[string], libset) tuple encoding the set
  of source files as absolute paths and the accumulated libset
  over all of those sources and their includes as discovered by doing
  an iterated preprocessor crawl starting from `main_src`.
  """
  
  # Transitively crawl all the files reachable from `main_src`. We
  # only preprocess them at this point, not compile. We also ask them
  # for the set of libraries they depend on and accumulate that into
  # a running aggregate "global" library set.
  @coroutine
  def fresh_src(src, libset0, libset1, incs_seen, srcs_seen):
    srcs_seen.add(src)
    
    incs, more_libset = yield includes_and_libset(src, libset0)
    libset_merge_inplace(libset1, more_libset)
    
    # Find adjacent source files by matching header names we
    # included against known source-file extensions.
    tasks = []
    for inc in incs:
      if inc not in incs_seen:
        incs_seen.add(inc)
        inc_base, _ = os.path.splitext(inc)
        for ext in find_src_exts(inc_base):
          src = inc_base + ext
          # Don't compile source files which have been included
          # into other source files.
          if src not in incs_seen and src not in srcs_seen:
            tasks.append(fresh_src(src, libset0, libset1, incs_seen, srcs_seen))
    
    yield async.when_succeeded(*tasks)

  # We repeatedly crawl the source files until we stop discovering
  # new library dependencies (i.e. libset reaches a fixed point). This
  # ensures that every reachable file has been preprocessed with all
  # the pp-flags from all the libraries discovered. This way everyone
  # gets a chance to test "#ifdef UPCXX_BACKEND" etc.
  libset = {}
  while 1:
    libset1 = dict(libset)
    incs_seen = set()
    srcs_seen = set()
    yield fresh_src(main_src, libset, libset1, incs_seen, srcs_seen)
    
    if len(libset) == len(libset1):
      break
    libset = libset1
  
  yield (srcs_seen, libset)

@rule(cli='obj', path_arg='src')
@coroutine
def compiled_object_of(cxt, src, main_src):
  """
  Hooks exists for cli-user only. Allows them to get the object file
  derived from a source file and main_source file.

  Usage: nobs obj <src> <main_src>
  """
  main_src = os.path.abspath(main_src)
  
  srcs, libset = yield _source_discovery(
    main_src,
    # includes_and_libset
    lambda src,libset: _includes_and_libset(cxt, src, libset),
    # find_src_exts (with depend_files=nop)
    lambda base: _find_src_exts(base, lambda*x:())
  )
  yield cxt.compile(src, libset)

@rule_memoized(path_arg=0)
class objects_and_libset:
  """
  Compile the given source file as well as all source files which can
  be found as sharing its name with a header included by any source
  file reached in this process (transitively closed set). Return pair
  containing set of all object files and the accumulated library
  dependency set.
  """
  version_bump = 4
  
  @traced
  def main_src(me, cxt, main_src):
    return main_src
  
  @traced
  def includes_and_libset(me, cxt, main_src, src, libset0):
    return _includes_and_libset(cxt, src, libset0)
  
  @traced
  @coroutine
  def compile(me, cxt, main_src, src, libset):
    if 0:
      # TODO: reduce libset to only the libraries needed by the source
      # file and its included headers.
      pass
    else:
      pp_libset = libset
    
    yield cxt.compile(src, pp_libset)
  
  @traced
  def find_src_exts(me, cxt, main_src, base):
    return _find_src_exts(base, me.depend_files)
  
  @coroutine
  def execute(me):
    srcs, libset = yield _source_discovery(
      me.main_src(),
      me.includes_and_libset,
      # find_src_exts
      lambda base: me.find_src_exts(base)
    )
    
    # Crawl is complete, now we compile each source file into an object.
    objs_fu = map(lambda src: me.compile(src, libset), srcs)
    yield async.when_succeeded(*objs_fu)
    objs = map(lambda fu:fu.value(), objs_fu)
    
    # Return pair the object list and global library set.
    yield (objs, libset)

@rule_memoized(cli='exe', path_arg=0)
class executable:
  """
  Compile the given source file as well as all source files which can
  be found as sharing its name with a header included by any source
  file reached in this process (transitively closed set). Take all those
  compiled object files and link them along with their library
  dependencies to proudce an executable. Path to executable returned.
  """
  version_bump = 2
  
  @traced
  def cxx(me, cxt, main_src):
    return cxt.cxx()
  
  @traced
  def ldflags(me, cxt, main_src):
    return cxt.ldflags()
  
  @traced
  def objects_and_libset(me, cxt, main_src):
    return cxt.objects_and_libset(main_src)
  
  @coroutine
  def execute(me):
    objs, libset = yield me.objects_and_libset()
    
    # link
    exe = me.mkpath('exe', suffix='.x')
    
    ld = libset_ld(libset)
    if ld is None:
      ld = yield me.cxx()
    
    ldflags = me.ldflags() + libset_ldflags(libset)
    libflags = libset_libflags(libset)
    
    yield subexec.launch(ld + ldflags + ['-o',exe] + objs + libflags)
    yield exe

@rule_memoized(cli='lib', path_arg=0)
class library:
  """
  Like executable(), but instead of linking the objects, stuffs them into
  a library archive and returns that and all the metadata as a libset.
  """
  version_bump = 9
  
  @traced
  def main_src(me, cxt, main_src):
    return main_src
  
  @traced
  def includes(me, cxt, main_src, src, libset):
    return cxt.includes(src, libset)
  
  @traced
  def objects_and_libset(me, cxt, main_src):
    return cxt.objects_and_libset(main_src)
  
  @traced
  def include_vdirs_and_tree(me, cxt, main_src):
    return futurize(
      cxt.include_vdirs(main_src),
      cxt.include_vdirs_tree(main_src)
    )
  
  @coroutine
  def execute(me):
    main_src = me.main_src()
    top_dir = here()
    
    objs, libset = yield me.objects_and_libset()

    if 0:
      # TODO: reduce libset to just libraries needed to preprocess `main_src`
      pass
    else:
      pp_libset = libset
    
    # Headers pulled in from main file. Discard those not within this
    # repo (top_dir) or the nobs artifact cache (path_art).
    incs = yield me.includes(main_src, libset)
    incs = map(os_extra.realpath, incs)
    incs = [i for i in incs if
      path_within_dir(i, top_dir) or
      path_within_dir(i, me.memodb.path_art)
    ]
    incs = list(set(incs))
    
    inc_vdirs, inc_vdirs_tree = yield me.include_vdirs_and_tree()
    
    # Reconstruct paths to includes as relative to vdirs because the
    # symlinks get squashed out by includes().
    incs1 = []
    updir = '..' + os.path.sep
    for inc in incs:
      candidates = []
      for vsym, vreal in inc_vdirs.items():
        rel = os.path.relpath(inc, vreal)
        if not rel.startswith(updir):
          candidates.append(os.path.join(inc_vdirs_tree, vsym, rel))
      candidates.sort(key=len)
      incs1.append(candidates[0] if len(candidates) != 0 else inc)
    incs = incs1
    
    par_dir = me.mkpath(key=None)
    os.makedirs(par_dir)
    
    libname, _ = os.path.splitext(main_src)
    libname = os.path.basename(libname)
    
    libpath = os.path.join(par_dir, 'lib' + libname + '.a')
    
    # archive objects to `libpath`
    yield subexec.launch(['ar', 'rcs', libpath] + objs)
    
    yield (
      libname,
      libset_merge(libset,
        {libname: {
          'incdirs': [inc_vdirs_tree],
          'incfiles': incs,
          'ppdeps': list(pp_libset.keys()),
          'libfiles': [libpath],
          'libdeps': list(libset.keys())
        }}
      )
    )

########################################################################
## Big user commands
########################################################################

@rule(cli='run', path_arg='main_src')
@coroutine
def run(cxt, main_src, *args):
  """
  Build the executable for `main_src` and run it with the given
  argument list `args`.
  """
  _, libset = yield cxt.objects_and_libset(main_src)
  exe = yield cxt.executable(main_src)

  if 'gasnet' in libset:
    meta = libset['gasnet']['meta']
    
    env1 = dict(os.environ)
    env1['GASNET_PREFIX'] = meta.get('GASNET_INSTALL') or meta.get('GASNET_BUILD')
    if meta['GASNET_CONDUIT'] == 'udp':
      env1['GASNET_SPAWNFN'] = env('GASNET_SPAWNFN','L')
    
    upcxx_run = here('utils','upcxx-run')
    ranks = str(env('RANKS', 1))
    
    def spawn():
      os.execve(upcxx_run, [upcxx_run, '-n', ranks, exe] + map(str, args), env1)
  else:
    def spawn():
      os.execv(exe, [exe] + map(str, args))

  # defer execv until after regular shutdown logic
  import atexit
  atexit.register(spawn)
  
  yield None

@rule(cli='install', path_arg='main_src')
@coroutine
def install(cxt, main_src, install_path):
  libname, libset = yield cxt.library(main_src)
  cc = yield cxt.cc()
  cxx = yield cxt.cxx()
  cxx11 = yield cxt.lang_cxx11()
  
  install_libset(install_path, libname, libset, meta_extra={
    'CC': cc,
    'CXX': cxx,
    'CXXFLAGS': cxx11
  })
  
  yield None

########################################################################
## GASNet build recipes                                               ##
########################################################################

@rule_memoized()
class gasnet_source:
  """
  Download and extract gasnet source tree.
  """
  version_bump = 0
  
  @traced
  def get_gasnet_user(me, cxt):
    return cxt.gasnet_user()
  
  @coroutine
  def execute(me):
    kind, value = me.get_gasnet_user()
    assert kind in ('tarball-url', 'tarball', 'source', 'build')
    
    if kind == 'source':
      source_dir = value
    
    elif kind == 'build':
      build_dir = value
      makefile = os.path.join(build_dir, 'Makefile')
      source_dir = makefile_extract(makefile, 'TOP_SRCDIR')
    
    else: # kind in ('tarball','tarball-url')
      if kind == 'tarball':
        tgz = value
        me.depend_files(tgz) # in case the user changes the tarball but not its path
      else: # kind == 'tarball-url'
        url = value
        tgz = me.mktemp()
        yield download(url, to=tgz)
        
      untar_dir = me.mkpath(key=None)
      os.makedirs(untar_dir)
      
      import tarfile
      with tarfile.open(tgz) as f:
        source_dir = os.path.join(untar_dir, f.members[0].name)
        f.extractall(untar_dir)
    
    yield source_dir

@rule_memoized()
class gasnet_config:
  """
  Returns (argv:list, env:dict) pair corresponding to the context
  in which gasnet's other/contrib/cross-configure-{xxx} script runs
  configure.
  """
  version_bump = 0
  
  @traced
  @coroutine
  def get_cross_and_gasnet_src(me, cxt):
    cross = env('CROSS', None)
    kind, value = cxt.gasnet_user()
    
    if cross and kind == 'install':
      raise errorlog.LoggedError(
        'Configuration Error',
        'It is invalid to use both cross-compile (CROSS) and ' +
        'externally installed gasnet (GASNET).'
      )
    
    gasnet_src = None
    if cross:
      gasnet_src = yield cxt.gasnet_source()
    
    yield (cross, gasnet_src)
  
  @traced
  def get_env(me, cxt, name):
    """Place dependendcies on environment variables."""
    return env(name, None)
  
  @coroutine
  def execute(me):
    cross, gasnet_src = yield me.get_cross_and_gasnet_src()
    
    if cross is None:
      yield ([], {})
      return
    
    # add "canned" env-var dependencies of scripts here
    if cross == 'cray-aries-slurm':
      me.get_env('SRUN')
    elif cross == 'bgq':
      me.get_env('USE_GCC')
      me.get_env('USE_CLANG')
    
    path = os.path.join
    crosslong = 'cross-configure-' + cross
    crosspath = path(gasnet_src, 'other', 'contrib', crosslong)
    
    if not os_extra.exists(crosspath):
      raise errorlog.LoggedError('Configuration Error', 'Invalid GASNet cross-compile script name (%s).'%cross)
    
    # Create a shallow copy of the gasnet source tree minus the
    # "configure" file.
    tmpd = me.mkdtemp()
    os_extra.mktree(
      tmpd,
      dict([
          (x, path(gasnet_src, x))
          for x in os_extra.listdir(gasnet_src)
          if x != 'configure'
        ] +
        [(crosslong, crosspath)]
      ),
      symlinks=True
    )
    
    # Add our own shim "configure" which will reap the command line args
    # and environment variables and punt them back to stdout.
    with open(path(tmpd, 'configure'), 'w') as f:
      f.write(
"""#!/usr/bin/env python
import os
import sys
sys.stdout.write(repr((sys.argv, os.environ)))
""")
    os.chmod(path(tmpd, 'configure'), 0777)
    
    # Run the cross-configure script.
    import subprocess as subp
    p = subp.Popen([path(tmpd, crosslong)], cwd=tmpd, stdout=subp.PIPE, stdin=subp.PIPE, stderr=subp.STDOUT, close_fds=True)
    out, _ = p.communicate('')
    if p.returncode != 0:
      raise errorlog.LoggedError('Configuration Error', 'GASNet cross-compile script (%s) failed.'%cross)
    argv, env = eval(out)
    
    # Skip the first argument since that's just "configure".
    argv = argv[1:]
    
    # Only record the environment delta.
    keep = ('CC','CXX','HOST_CC','HOST_CXX',
            'MPI_CC','MPI_CFLAGS','MPI_LIBS','MPIRUN_CMD')
    env0 = os.environ
    for x in env0:
      if x in keep: continue
      if x.startswith('CROSS_'): continue
      if x not in env: continue
      if env[x] != env0[x]: continue
      del env[x]
    
    yield (argv, env)

@rule_memoized()
class gasnet_configured:
  """
  Returns a configured gasnet build directory.
  """
  version_bump = 4
  
  @traced
  def get_gasnet_user(me, cxt):
    return cxt.gasnet_user()
  
  @traced
  @coroutine
  def get_config(me, cxt):
    flags = ['-O%d'%cxt.cg_optlev_default()] + (['-g'] if cxt.cg_dbgsym() else []) 
    
    cc = yield cxt.cc()
    cc = cc + flags
    
    cc_ver = version_of(cc)
    me.depend_fact(key='CC', value=cc_ver)
    
    cxx = yield cxt.cxx()
    cxx = cxx + flags
    
    cxx_ver = version_of(cxx)
    me.depend_fact(key='CXX', value=cxx_ver)
    
    config = yield cxt.gasnet_config()
    source_dir = yield cxt.gasnet_source()
    
    debug = cxt.gasnet_debug()
    user_args = shplit(env('GASNET_CONFIGURE_ARGS',''))
    
    yield (cc, cxx, debug, config, source_dir, user_args)
  
  @coroutine
  def execute(me):
    kind, value = yield me.get_gasnet_user()
    
    if kind == 'build':
      build_dir = value
    else:
      cc, cxx, debug, config, source_dir, user_args = yield me.get_config()
      config_args, config_env = config
      build_dir = me.mkpath(key=None)
      os.makedirs(build_dir)
      
      env1 = dict(os.environ)
      env1.update(config_env)
      
      if 'CC' not in env1:
        env1['CC'] = ' '.join(cc)
      if 'CXX' not in env1:
        env1['CXX'] = ' '.join(cxx)

      # gasnet can't handle `-fsanitize=xxx`, so we just drop it
      # TODO: Instead of dropping the flag, we can suppress its effect with:
      # env['ASAN_OPTIONS'] = 'detect_leaks=0'
      env1['CXX'] = ' '.join([s for s in env1['CXX'].split() if not s.startswith('-fsanitize=')])
    
      # these arguments enforce GASNet defaults that UPC++ relies upon
      # to ensure that users cannot break our dependencies by setting GASNET_CONFIGURE_ARGS
      # For details, see GASNet configure --help
      misc_conf_opts = [
        '--disable-parsync', '--enable-seq', '--enable-par',
        '--enable-pthreads',
        '--disable-segment-everything',
      ]
      
      print>>sys.stderr, 'Configuring GASNet...'
      yield subexec.launch(
        [os.path.join(source_dir, 'configure')] +
        config_args +
        user_args + 
        misc_conf_opts +
        (['--enable-debug'] if debug else ['--disable-debug']),
        
        cwd = build_dir,
        env = env1
      )
    
    yield build_dir

@rule_memoized(cli='gasnet_configured_conduits')
class gasnet_configured_conduits:
  """
  Get the list of detected conduits from a configured gasnet.
  """
  version_bump = 4
  
  @traced
  def get_config(me, cxt):
    kind, value = cxt.gasnet_user()
    return futurize(
      kind,
      value if kind == 'install' else cxt.gasnet_configured()
    )
  
  @coroutine
  def execute(me):
    kind, dir_path = yield me.get_config()
    
    header = os.path.join(*(
      [dir_path] +
      (['include'] if kind == 'install' else []) +
      ['gasnet_config.h']
    ))
    
    import re
    conduits = None
    with open(header,'r') as f:
      for line in f:
        m = re.match('[ \t]*#define +GASNETI_CONDUITS +"([^"]*)"', line)
        if m is not None:
          conduits = m.group(1).split()
          break
        elif 'GASNETI_CONDUITS' in line:
          print>>sys.stderr, repr(line)
    
    assert conduits is not None
    yield conduits
    
@rule_memoized(cli='gasnet_built')
class gasnet_built:
  """
  Build gasnet (if necessary). Return tuple (installed, built_dir) where
  `installed` is a boolean indicating whether `built_dir` points to a
  gasnet install tree or build tree.
  """
  version_bump = 0
  
  @traced
  def get_config(me, cxt):
    kind, value = cxt.gasnet_user()
    install_to = cxt.gasnet_install_to()
    return futurize(
      kind,
      value if kind == 'install' else cxt.gasnet_configured(),
      install_to,
      None if install_to else cxt.gasnet_conduit(),
      None if install_to else cxt.gasnet_syncmode()
    )
  
  @traced
  def cxx(me, cxt):
    return cxt.cxx()
  
  @coroutine
  def execute(me):
    kind, build_or_install_dir, install_to, conduit, syncmode \
      = yield me.get_config()
    
    if kind == 'install':
      installed = True
      built_dir = build_or_install_dir
    else:
      # We weren't given an installed gasnet, so we're looking at a
      # configured build dir.
      build_dir = build_or_install_dir
      
      if install_to is None:
        # We haven't been told to install gasnet to a specific location,
        # so we can build just what we need (conduit,threading)
        print>>sys.stderr, 'Building GASNet (conduit=%s, threading=%s)...'%(conduit, syncmode)
        yield subexec.launch(
          upcxx_make() + [syncmode],
          cwd = os.path.join(build_dir, '%s-conduit'%conduit)
        )
        
        if conduit == 'udp':
          yield subexec.launch(
            upcxx_make() + ['amudprun'],
            cwd = os.path.join(build_dir, 'other', 'amudp')
          )
        
        installed = False
        built_dir = build_or_install_dir
      else:
        # User wants us to install gasnet
        print>>sys.stderr, 'Building GASNet...'
        yield subexec.launch(upcxx_make(), cwd=build_dir)
        
        print>>sys.stderr, 'Installing GASNet...'
        yield subexec.launch(
          upcxx_make() + ['install', 'prefix='+install_to],
          cwd=build_dir
        )
        installed = True
        built_dir = install_to
    
    yield (installed, built_dir)

@rule_memoized(cli='gasnet')
class gasnet:
  """
  Builds/installs gasnet as necessary. Returns library dependencies dictionary.
  """
  version_bump = 14
  
  @traced
  @coroutine
  def get_config(me, cxt):
    installed, built_dir = yield cxt.gasnet_built()
    cxx = yield cxt.cxx()
    yield (
      installed, built_dir,
      cxt.gasnet_conduit(),
      cxt.gasnet_syncmode(),
      cxx
    )

  @traced
  def includes(me, cxt, src, libset):
    return cxt.includes(src, libset)
  
  @coroutine
  def execute(me):
    import os

    installed, built_dir, conduit, syncmode, cxx = yield me.get_config()
    
    makefile = os.path.join(*(
      [built_dir] +
      (['include'] if installed else []) +
      ['%s-conduit'%conduit, '%s-%s.mak'%(conduit, syncmode)]
    ))
    
    GASNET_LD = shplit(makefile_extract(makefile, 'GASNET_LD'))
    GASNET_LDFLAGS = shplit(makefile_extract(makefile, 'GASNET_LDFLAGS'))
    GASNET_CXXCPPFLAGS = shplit(makefile_extract(makefile, 'GASNET_CXXCPPFLAGS'))
    GASNET_CXXFLAGS = shplit(makefile_extract(makefile, 'GASNET_CXXFLAGS'))
    GASNET_LIBS = shplit(makefile_extract(makefile, 'GASNET_LIBS'))
    
    # workaround for GASNet not giving us a C++ capable linker.
    GASNET_LD = cxx + GASNET_LD[1:]
    
    if installed:
      # use gasnet install in-place
      incdirs = []
      incfiles = []
      libfiles = []
    else:
      makefile = os.path.join(built_dir, 'Makefile')

      if 0:
        # Giving up on identifying each gasnet header the user might need.
        # EVEN THOUGH this is the right (and working) way to do it.
        
        dummy_fd, dummy_path = me.mkstemp(suffix='.cpp')
        os.write(dummy_fd,
          "#include <gasnet.h>\n" +
          "#include <gasnetex.h>\n" +
          "#include <gasnet_tools.h>\n"
        )
        os.close(dummy_fd)
        
        incfiles = yield me.includes(
          dummy_path,
          {'gasnet': {'ppflags': GASNET_CXXCPPFLAGS}}
        )
        
        # pull "-I..." arguments out of GASNET_CXXCPPFLAGS
        incdirs = [x for x in GASNET_CXXCPPFLAGS if x.startswith('-I')]
        GASNET_CXXCPPFLAGS = [x for x in GASNET_CXXCPPFLAGS if x not in incdirs]
        incdirs = [x[2:] for x in incdirs] # drop "-I" prefix
        
      else:
        # Instead we bring no headers. This will break users trying to
        # include gasnet from a upcxx `install --single` tree.
        incdirs = []
        incfiles = []
        # Leaving "-I..." flags in GASNET_CXXCPPFLAGS even though they'
        # will all point into a build directory which might be in
        # .nobs.
      
      # pull "-L..." arguments out of GASNET_LIBS, keep only the "..."
      libdirs = [x[2:] for x in GASNET_LIBS if x.startswith('-L')]
      # pull "-l..." arguments out of GASNET_LIBS, keep only the "..."
      libnames = [x[2:] for x in GASNET_LIBS if x.startswith('-l')]
      
      # filter libdirs for those made by gasnet
      libdirs = [x for x in libdirs if path_within_dir(x, built_dir)]
      
      # find libraries in libdirs
      libfiles = []
      libnames_matched = set()
      for libname in libnames:
        lib = 'lib' + libname + '.a'
        for libdir in libdirs:
          libfile = os.path.join(libdir, lib)
          if os.path.exists(libfile):
            # assert same library not found under multiple libdir paths
            assert libname not in libnames_matched
            libfiles.append(libfile)
            libnames_matched.add(libname)
      
      # prune extracted libraries from GASNET_LIBS
      GASNET_LIBS = [x for x in GASNET_LIBS
        if not(
          (x.startswith('-L') and x[2:] in libdirs) or
          (x.startswith('-l') and x[2:] in libnames_matched)
        )
      ]

    if 0:
      # Pull all flags like "-DGASNET*" out of CXXCPPFLAGS. These will
      # go into the 'ppdefs' of the libset, remaining flags into 'ppflags'.
      import re
      ppdefs = [s for s in GASNET_CXXCPPFLAGS if s.startswith('-DGASNET')]  
      GASNET_CXXCPPFLAGS = [s for s in GASNET_CXXCPPFLAGS if s not in ppdefs]
      ppdefs = [re.match('-D(GASNET[a-zA-Z0-9_]*)(=(.*))?', s) for s in ppdefs]
      ppdefs = dict((m.group(1), m.group(3) or '') for m in ppdefs)
    else:
      ppdefs = {}
    
    yield {
      'gasnet': {
        'meta': {
          'GASNET_CONDUIT': conduit,
          'GASNET_INSTALL' if installed else 'GASNET_BUILD': built_dir
        },
        'incdirs': incdirs,
        'incfiles': incfiles,
        'ld': GASNET_LD,
        'ldflags': GASNET_LDFLAGS,
        'ppflags': GASNET_CXXCPPFLAGS,
        'ppdefs': ppdefs,
        'cgflags': GASNET_CXXFLAGS,
        'libfiles': libfiles,
        'libflags': GASNET_LIBS,
        'libdeps': [] # all dependencies flattened into libflags by gasnet
      }
    }

########################################################################
## libset_*** routines                                                ##
########################################################################

def libset_merge_inplace(dst, src):
  """
  Merge libraries of `src` into `dst`.
  """
  for k,srec in src.items():
    if srec != dst.get(k, srec):
      raise Exception("Multiple '%s' libraries with differing configurations." % k)
    dst[k] = srec

def libset_merge(*libsets):
  """
  Combine series of libsets into one returned libset.
  """
  ans = {}
  for x in libsets:
    libset_merge_inplace(ans, x)
  return ans

def libset_closure(libset, depsfield, rootnames):
  ans = dict((x,libset[x]) for x in rootnames)
  more = list(rootnames)
  
  while len(more) != 0:
    a = more.pop()
    for b in libset.get(a, {}).get(depsfield, ()):
      if b not in ans:
        ans[b] = libset.get(b, {})
        more.append(b)
  
  return ans

def libset_ppdefs(libset):
  """
  Return the aggregate dict of preprocessor macros needed by all libraries.
  """
  ppdefs = {}
  for rec in libset.values():
    for x,y in rec.get('ppdefs', {}).items():
      y0 = ppdefs.get(x,y)
      if y != y0:
        raise Exception("Conflicting values for preprocessor macro definition %s=%s and %s=%s."%(x,y,x,y0))
      ppdefs[x] = y
  return ppdefs

def libset_ppflags(libset):
  """
  Return the aggregate list of preprocessor flags needed by all libraries.
  Includes all information from: incfiles, incdirs, ppdefs, ppflags.
  """
  flags = []

  ppdefs = libset_ppdefs(libset)
  for x,y in sorted(ppdefs.items()):
    if y is not None:
      flags.append('-D%s=%s'%(x,y))
  
  for rec in libset.values():
    flags.extend(rec.get('ppflags', []))
  
  for rec in libset.values():
    for d in rec.get('incdirs', []):
      flag = '-I' + d
      if flag not in flags:
        flags.append(flag)
  
  return flags

def libset_cgflags(libset):
  flags = []
  for rec in libset.values():
    flags.extend(rec.get('cgflags', []))
  return flags

def libset_ld(libset):
  lds = set(tuple(x.get('ld',())) for x in libset.values())
  lds.discard(())
  if len(lds) == 0:
    return None
  if len(lds) != 1:
    raise Exception("Multiple linkers demanded:" + ''.join(map(lambda x:'\n  '+' '.join(x), lds)))
  return list(lds.pop())

def libset_ldflags(libset):
  flags = []
  for rec in libset.values():
    flags.extend(rec.get('ldflags', []))
  return flags

def libset_libflags(libset):
  """
  Generate link-line library flags from a topsort over
  library-library dependencies.
  """
  sorted_lpaths = []
  sorted_flags = []
  visited = set()
  
  def topsort(xs):
    for x in xs:
      rec = libset.get(x, {})
      
      topsort(rec.get('libdeps', []))
      
      if x not in visited:
        visited.add(x)
        
        libfiles = rec.get('libfiles', [])
        libflags = rec.get('libflags', [])
        
        sorted_lpaths.append(
          ['-L' + os.path.dirname(f) for f in libfiles]
        )
        sorted_flags.append(
          ['-l' + os.path.basename(f)[3:-2] for f in libfiles] +
          libflags
        )
  
  def uniquify(xs):
    ys = []
    for x in xs:
      if x not in ys:
        ys.append(x)
    return ys
  
  topsort(libset)
  
  sorted_lpaths.reverse()
  sorted_lpaths = sum(sorted_lpaths, [])
  sorted_lpaths = uniquify(sorted_lpaths)
  
  sorted_flags.reverse()
  sorted_flags = sum(sorted_flags, [])
  
  return sorted_lpaths + sorted_flags

def install_libset(install_path, name, libset, meta_extra={}):
  """
  Install a library set to the given path. Produces headers and binaries
  in the typical "install_path/{bin,include,lib}" structure. Also
  creates a metadata retrieval script "bin/${name}-meta" (similar in
  content to a pkgconfig script) for querying the various compiler and
  linker flags.
  """
  import os
  import shutil
  
  base_of = os.path.basename
  join = os.path.join
  updir = '..' + os.path.sep
  
  class InstallError(Exception):
    pass
  
  rollback = []
  commit = []
  suffix = '.771b861d-97e2-49db-b3a2-6d99437726bf'
  
  def ensure_dirs_upto(path):
    head, tail = os.path.split(path)
    if head=='' or os.path.isdir(head):
      return
    if os.path.exists(head):
      raise InstallError('Path "%s" is not a directory.'%head)
    
    ensure_dirs_upto(head)
    
    try:
      os.mkdir(head)
    except OSError as e:
      raise InstallError('Failed to create directory "%s": %s'%(head,e.message))
    
    rollback.append(lambda: os.rmdir(head))
  
  def install_file(src, dst, mode, src_is_path_not_contents=True):
    isfile = os.path.isfile(dst)
    if isfile:
      try:
        os.rename(dst, dst+suffix)
      except OSError as e:
        raise InstallError('Failed to rename "%s": %s".'%(dst,e.message))
      rollback.append((lambda dst: lambda: os.rename(dst+suffix, dst))(dst))
      commit.append((lambda dst: lambda: os.remove(dst+suffix))(dst))
    elif os.path.exists(dst):
      raise InstallError('Path "%s" is not a file.'%path)
    else:
      ensure_dirs_upto(dst)
    
    if not isfile:
      rollback.append((lambda dst: lambda: os.remove(dst))(dst))
    
    try:
      if src_is_path_not_contents:
        shutil.copyfile(src, dst)
      else:
        with open(dst, 'w') as f:
          f.write(src)
      os.chmod(dst, mode)
    except Exception as e:
      raise InstallError('Could not write file "%s": %s'%(dst, e.message))
  
  def install_contents(contents, dst, mode):
    install_file(contents, dst, mode, src_is_path_not_contents=False)
  
  try:
    ####################################################################
    # produce a new version of libset representing the post-install libset
    # (with the headers and library files moved)
    installed_libset = {}
    empty_lib = {'libflags':[]}
    
    ####################################################################
    # copy/transform preprocessor stuff from libset to installed_libset
    
    for xname,rec in libset_closure(libset, 'ppdeps', [name]).items():
      incdirs = rec.get('incdirs', [])
      incfiles = rec.get('incfiles', [])
      
      incfiles1 = []
      for incf in incfiles:
        # copy for each non-upwards relative path
        for incd in reversed(incdirs):
          rel = os.path.relpath(incf, incd)
          if not rel.startswith(updir):
            # copy include file to relative path under "install_path/include"
            src = join(incd, rel)
            dst = join(install_path, 'include', rel)
            incfiles1.append(dst)
            install_file(src, dst, 0644)

      rec1 = dict(empty_lib)
      rec1.update({
        'incdirs': [join(install_path, 'include')],
        'incfiles': incfiles1,
      })
      if 'ppflags' in rec: rec1['ppflags'] = rec['ppflags']
      if 'ppdefs'  in rec: rec1['ppdefs'] = rec['ppdefs']
      
      installed_libset[xname] = rec1
    
    ####################################################################
    # copy/transform library stuff from libset to installed_libset
    
    libfiles_all = []
    
    for xname,rec in libset_closure(libset, 'libdeps', [name]).items():
      rec1 = installed_libset.get(xname, dict(empty_lib))
      installed_libset[xname] = rec1
      
      if 'libfiles' in rec:
        libfiles = rec['libfiles']
        libfiles_all.extend(libfiles)
        rec1['libfiles'] = [join(install_path, 'lib', base_of(f)) for f in libfiles]
      
      if 'libflags' in rec: rec1['libflags'] = rec['libflags']
      if 'ldflags'  in rec: rec1['ldflags'] = rec['ldflags']
    
    if len(libfiles_all) != len(set(map(base_of, libfiles_all))):
      raise InstallError(
        'Duplicate library names in list:\n  ' + '\n  '.join(libfiles_all)
      )
    
    for src in libfiles_all:
      dst = join(install_path, 'lib', base_of(src))
      install_file(src, dst, 0644)
    
    ####################################################################
    # copy over all the other fields from libset into installed_libset
    
    for xname,rec1 in installed_libset.items():
      ignore = (
        'incfiles','incdirs','ppflags','ppdefs',
        'libfiles','libflags','ldflags'
      )
      rec1.update(
        dict((x,y) for x,y in libset.get(xname,{}).items() if x not in ignore)
      )
    
    ####################################################################
    # produce metadata script

    # build dict of library provided meta-assignments
    metas = dict(meta_extra)
    for rec in installed_libset.values():
      for x,y in rec.get('meta',{}).items():
        assert y == metas.get(x,y) # libraries provided conflicting values for same meta-varaible
        metas[x] = y
    
    def flat(*xs):
      if len(xs) == 1:
        if xs[0] is None:
          return ''
        elif type(xs[0]) in (tuple, list):
          return flat(*xs[0])
        else:
          return str(xs[0])
      else:
        return ' '.join([x for x in [flat(x) for x in xs] if x != ''])
    
    for k,v in dict(
        CPPFLAGS = libset_ppflags(installed_libset),
        CXXFLAGS = libset_cgflags(installed_libset),
        LDFLAGS = libset_ldflags(installed_libset),
        LIBS = libset_libflags(installed_libset)
      ).items():
      metas[k] = flat(metas.get(k), v)
    
    meta_path = join(install_path, 'bin', name+'-meta')
    meta_contents = \
'''#!/bin/sh
''' + '\n'.join(
  ["%s='%s'"%(k,flat(v)) for k,v in sorted(metas.items()) if flat(v) != '']
) + '''
PPFLAGS="$CPPFLAGS"
LIBFLAGS="$LIBS"

[ "$1" != "" ] && eval echo '$'"$1"
'''
    install_contents(meta_contents, meta_path, 0755)
  
  except Exception as e:
    for fn in reversed(rollback):
      try: fn()
      except OSError: pass
    
    if isinstance(e, InstallError):
      raise errorlog.LoggedError('Installation to "%s" aborted: %s'%(install_path, e.message))
    else:
      raise
  
  else:
    for fn in commit:
      fn()

########################################################################
## Utilties                                                           ##
########################################################################

def env(name, otherwise, universe=None):
  """
  Read `name` from the environment returning it as an integer if it
  looks like one otherwise a string. If `name` does not exist in the 
  environment then `otherwise` is returned.
  """
  try:
    got = os.environ[name]
    if got == '':
      got = otherwise
    else:
      try: got = int(got)
      except ValueError: pass
  except KeyError:
    got = otherwise
  
  if universe is not None and got != otherwise and got not in universe:
    raise errorlog.LoggedError('%s must be one of: %s'%(name, ', '.join(universe)))
  
  return got

def makefile_extract(makefile, varname):
  """
  Extract a variable's value from a makefile.
  --no-print-directory is required to ensure correct behavior when nobs was invoked by make
  """
  import subprocess as sp
  p = sp.Popen(upcxx_make() + ['--no-print-directory','-f','-','gimme'], stdin=sp.PIPE, stdout=sp.PIPE, stderr=sp.PIPE, close_fds=True)
  tmp = ('include {0}\n' + 'gimme:\n' + '\t@echo $({1})\n').format(makefile, varname)
  val, _ = p.communicate(tmp)
  if p.returncode != 0:
    raise Exception('Makefile %s not found.'%makefile)
  val = val.strip(' \t\n')
  return val

def path_within_dir(path, dirpath):
  rel = os.path.relpath(path, dirpath)
  return path == dirpath or not rel.startswith('..' + os.path.sep)

@cached
def output_of(cmd_args):
  """
  Returns (returncode,stdout,stderr) generated by invoking the command
  arguments as a child process.
  """
  try:
    import subprocess as sp
    p = sp.Popen(cmd_args, stdin=sp.PIPE, stdout=sp.PIPE, stderr=sp.PIPE, close_fds=True)
    stdout, stderr = p.communicate()
    return (p.returncode, stdout, stderr)
  except OSError as e:
    return (e.errno, None, None)

def download(url, to):
  def doit():
    import urllib
    try:
      urllib.urlretrieve(url, to)
    except IOError as e:
      if e.args[0] == 'socket error':
        raise errorlog.LoggedError(
  'Internet troubles.',
  'Socket error "%s" when attempting to download "%s".\n'%(e.args[1].strerror, url)
        )
      else:
        raise
  
  print>>sys.stderr, 'Downloading %s' % url
  doit()
  print>>sys.stderr, 'Finished    %s' % url
