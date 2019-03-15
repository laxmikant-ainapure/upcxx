/*  bupc_tentative.h
 *
 *  Header file for C programs that want to interoperate with UPC code.
 *  This header exposes a subset of the bupc_extern.h entry points as 
 *  tentative definitions, allowing C programs to access them conditionally
 *  upon linkage with libupcr. The basic idea is when libupcr is linked,
 *  all the function pointers in this header will reference the bupc_extern API,
 *  and otherwise they will all be NULL.
 *  See bupc_extern.h for usage documentation on the corresponding entry points.
 *
 *  The mechanisms in this header file are not part of the UPC standard, and
 *  are extensions particular to the Berkeley UPC system.  They should be
 *  considered to have 'experimental' status, and may be changed.
 *
 *  The authoritative version of this header lives here:
 *  $Source: bitbucket.org:berkeleylab/upc-runtime.git/bupc_tentative.h $
 *  Clients may embed and distribute this header, but should NOT modify
 *  any of the definitions.
 *
 *  This interface is available starting in UPCR v2018.5.3, runtime spec v3.13.
 *  
 *  Version history: (BUPC_TENTATIVE_VERSION_{MAJOR,MINOR})
 *  ---------------
 *  1.1 : Initial version
 */

#ifndef __BUPC_TENTATIVE_H
#define __BUPC_TENTATIVE_H

#ifdef __cplusplus
#error This header is not compatible with C++.
/* C++ does not support tentative definitions, which are a required mechanism for this header to work as intended.
 * C++ clients should use this header from C file linked into their system.
 */
#endif

// monotonic version tracking for the symbols below
// guaranteed to be non-zero
#define BUPC_TENTATIVE_VERSION_MAJOR 1
#define BUPC_TENTATIVE_VERSION_MINOR 1

/* These are INTENTIONALLY TENTATIVE definitions.
 * do NOT add extern/static storage qualifiers or initializers to any of these!!
 */

int bupc_tentative_version_major;
int bupc_tentative_version_minor;

void (*bupc_tentative_init)(int *argc, char ***argv); 

void (*bupc_tentative_init_reentrant)(int *argc, char ***argv, 
			 int (*pmain_func)(int, char **) ); 

void (*bupc_tentative_exit)(int exitcode);

int (*bupc_tentative_mythread)(void);
int (*bupc_tentative_threads)(void);

char * (*bupc_tentative_getenv)(const char *env_name);

void (*bupc_tentative_notify)(int barrier_id);
void (*bupc_tentative_wait)(int barrier_id);
void (*bupc_tentative_barrier)(int barrier_id);

void * (*bupc_tentative_alloc)(size_t bytes);
void * (*bupc_tentative_all_alloc)(size_t nblocks, size_t blocksz);
void (*bupc_tentative_free)(void *ptr, int thread);
void (*bupc_tentative_all_free)(void *ptr, int thread);

void (*bupc_tentative_config_info)(const char **upcr_config_str,
                                   const char **gasnet_config_str,
                                   const char **upcr_version_str,
                                   int    *upcr_runtime_spec_major,
                                   int    *upcr_runtime_spec_minor,
                                   int    *upcr_debug,
                                   int    *upcr_pthreads,
                                   size_t *upcr_pagesize);
#endif /* __BUPC_TENTATIVE_H */
