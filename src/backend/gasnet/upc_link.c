// upc_link.c
// this file MUST be compiled as C (not C++) to support the bupc_tentative API

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <upcxx/backend/gasnet/upc_link.h>
#include "bupc_tentative.h"

static gex_Rank_t upcxx_upc_rank_me = GEX_RANK_INVALID;
static gex_Rank_t upcxx_upc_rank_n = GEX_RANK_INVALID;
static int upcxx_upc_is_init = 0;

extern int upcxx_upc_is_linked(void) {
  int result = !!bupc_tentative_version_major;
  if (result) { // ensure our header clone is not out-of-date
    assert(bupc_tentative_version_major == BUPC_TENTATIVE_VERSION_MAJOR &&
           bupc_tentative_version_minor >= BUPC_TENTATIVE_VERSION_MINOR);
  }
  // sanity-check that the required symbols are present
  assert(result == !!bupc_tentative_init);
  assert(result == !!bupc_tentative_config_info);
  assert(result == !!bupc_tentative_alloc);
  assert(result == !!bupc_tentative_free);
  assert(result == !!bupc_tentative_all_alloc);
  assert(result == !!bupc_tentative_all_free);
  return result; 
}

extern void upcxx_upc_init(
                gex_Client_t           *client_p,
                gex_EP_t               *ep_p,
                gex_TM_t               *tm_p
            ) {
  assert(upcxx_upc_is_linked());
  assert(!upcxx_upc_is_init);

  // Query UPCR configuration information, to check compatibility
  const char *upcr_config_str = NULL;
  const char *gasnet_config_str = NULL;
  const char *upcr_version_str = NULL;
  int upcr_debug = 0;
  int upcr_pthreads = 0;
  bupc_tentative_config_info(&upcr_config_str, &gasnet_config_str, &upcr_version_str,
                             0, 0, &upcr_debug, &upcr_pthreads, 0);
  if (upcr_pthreads) {
    fprintf(stderr, "ERROR: UPC++ interoperability for Berkeley UPC Runtime does not support `upcc -pthreads` mode. Please compile your UPC code with `upcc -nopthreads`.\n");
    abort();
  }

  static int    dummy_argc = 1;
  static char dummy_exename[] = "upcxx_dummy";
  static char *_dummy_argv[] = { dummy_exename, NULL };
  static char **dummy_argv = _dummy_argv;
  bupc_tentative_init(&dummy_argc, &dummy_argv);

  // we currently assume UPCR performs client init with GEX_FLAG_USES_GASNET1
  gasnet_QueryGexObjects(client_p, ep_p, tm_p, NULL);
  assert(gex_Client_QueryFlags(*client_p) & GEX_FLAG_USES_GASNET1);

  upcxx_upc_rank_me = gex_TM_QueryRank(*tm_p);
  upcxx_upc_rank_n = gex_TM_QuerySize(*tm_p);
  assert(upcxx_upc_rank_n > 0);
  assert(upcxx_upc_rank_me < upcxx_upc_rank_n);

  upcxx_upc_is_init = 1;
}

extern void *upcxx_upc_alloc(size_t sz) {
  assert(upcxx_upc_is_linked());
  assert(upcxx_upc_is_init);

  void *ptr = bupc_tentative_alloc(sz);
  if (!ptr) {
    fprintf(stderr, 
            "FATAL ERROR: UPC++ failed to allocate %lld bytes from the Berkeley UPC Runtime non-collective shared heap\n", 
            (long long)sz);
    fflush(stderr);
    abort();
  }
  return ptr;
}

extern void *upcxx_upc_all_alloc(size_t sz) {
  assert(upcxx_upc_is_linked());
  assert(upcxx_upc_is_init);

  void *ptr = bupc_tentative_all_alloc(upcxx_upc_rank_n, sz);
  if (!ptr) {
    fprintf(stderr, 
            "FATAL ERROR: UPC++ failed to allocate %lld bytes from the Berkeley UPC Runtime collective shared heap\n", 
            (long long)sz);
    fflush(stderr);
    abort();
  }
  return ptr;
}


extern void upcxx_upc_free(void *ptr) {
  assert(upcxx_upc_is_linked());
  assert(upcxx_upc_is_init);

  bupc_tentative_free(ptr, upcxx_upc_rank_me);
}

extern void upcxx_upc_all_free(void *ptr) {
  assert(upcxx_upc_is_linked());
  assert(upcxx_upc_is_init);

  bupc_tentative_all_free(ptr, 0);
}


