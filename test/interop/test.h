#ifndef __INTEROP_TEST_H
#define __INTEROP_TEST_H

#ifdef __cplusplus
extern "C" {
#endif

// This call is used to init UPCR when main is not in UPC code.
// It's called indirectly by upcxx::init() when needed, but that 
// library lacks access to the real argc/argv of the process.
// For this reason, one may get more portable spawning behavior 
// by calling it explicitly early in main() with the real argc/argv.
extern void bupc_init(int *argc, char ***argv);

extern int test_upc(int input);
extern int test_upcxx(int input);

#ifdef __cplusplus
}
#endif

#endif
