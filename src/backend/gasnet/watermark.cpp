// tools-lite mode, to minimize resources for recompiling this TU
#undef GASNET_SEQ
#undef GASNET_PAR
#define GASNETT_LITE_MODE 1
#include <gasnet_tools.h>

#include <upcxx/version.hpp>

////////////////////////////////////////////////////////////////////////
// Library version watermarking
//
// This watermarking file is rebuilt on every rebuild of libupcxx.a
// so minimize the contents of this file to reduce compilation time.
//

#ifndef UPCXX_VERSION
#error  UPCXX_VERSION missing!
#endif
GASNETT_IDENT(UPCXX_IdentString_LibraryVersion, "$UPCXXLibraryVersion: " _STRINGIFY(UPCXX_VERSION) " $");
#ifndef UPCXX_SPEC_VERSION
#error  UPCXX_SPEC_VERSION missing!
#endif
GASNETT_IDENT(UPCXX_IdentString_SpecVersion, "$UPCXXSpecVersion: " _STRINGIFY(UPCXX_SPEC_VERSION) " $");

#ifndef UPCXX_GIT_VERSION
#include <upcxx/git_version.h>
#endif
#ifdef  UPCXX_GIT_VERSION
GASNETT_IDENT(UPCXX_IdentString_GitVersion, "$UPCXXGitVersion: " _STRINGIFY(UPCXX_GIT_VERSION) " $");
#endif

#if UPCXX_BACKEND_GASNET_SEQ
GASNETT_IDENT(UPCXX_IdentString_ThreadMode, "$UPCXXThreadMode: SEQ $");
#elif UPCXX_BACKEND_GASNET_PAR
GASNETT_IDENT(UPCXX_IdentString_ThreadMode, "$UPCXXThreadMode: PAR $");
#endif

#if GASNET_DEBUG
GASNETT_IDENT(UPCXX_IdentString_CodeMode, "$UPCXXCodeMode: debug $");
#else
GASNETT_IDENT(UPCXX_IdentString_CodeMode, "$UPCXXCodeMode: opt $");
#endif

GASNETT_IDENT(UPCXX_IdentString_GASNetVersion, "$UPCXXGASNetVersion: " 
              _STRINGIFY(GASNET_RELEASE_VERSION_MAJOR) "."
              _STRINGIFY(GASNET_RELEASE_VERSION_MINOR) "."
              _STRINGIFY(GASNET_RELEASE_VERSION_PATCH) " $");

#if UPCXX_CUDA_ENABLED
  GASNETT_IDENT(UPCXX_IdentString_CUDAEnabled, "$UPCXXCUDAEnabled: 1 $");
#else
  GASNETT_IDENT(UPCXX_IdentString_CUDAEnabled, "$UPCXXCUDAEnabled: 0 $");
#endif

GASNETT_IDENT(UPCXX_IdentString_AssertEnabled, "$UPCXXAssertEnabled: " _STRINGIFY(UPCXX_ASSERT_ENABLED) " $");

#if UPCXX_MPSC_QUEUE_ATOMIC
  GASNETT_IDENT(UPCXX_IdentString_MPSCQueue, "$UPCXXMPSCQueue: atomic $");
#elif UPCXX_MPSC_QUEUE_BIGLOCK
  GASNETT_IDENT(UPCXX_IdentString_MPSCQueue, "$UPCXXMPSCQueue: biglock $");
#endif

GASNETT_IDENT(UPCXX_IdentString_CompilerID, "$UPCXXCompilerID: " PLATFORM_COMPILER_IDSTR " $");

GASNETT_IDENT(UPCXX_IdentString_CompilerStd, "$UPCXXCompilerStd: " _STRINGIFY(__cplusplus) " $");

GASNETT_IDENT(UPCXX_IdentString_BuildTimestamp, "$UPCXXBuildTimestamp: " __DATE__ " " __TIME__ " $");

namespace upcxx { namespace backend { namespace gasnet {
extern int watermark_init();
}}}
// this function exists to ensure this object gets linked into the executable
int upcxx::backend::gasnet::watermark_init() {
  static volatile int dummy = 0;
  int tmp = dummy;
  tmp++;
  dummy = tmp;
  return tmp;
}
