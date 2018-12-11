#include <stdio.h>
#include <upc.h>

#include "test.h"

extern int test_upc(int input) {
  if (!MYTHREAD) printf("test_upc(%i): starting\n", input);
  upc_barrier;
  shared int *sip = upc_all_alloc(THREADS,sizeof(int));
  sip[MYTHREAD] = MYTHREAD;
  upc_barrier;
  int peer = (MYTHREAD+1)%THREADS; 
  int fetch = sip[peer];
  if (fetch != peer) {
    fprintf(stderr,"%i: ERROR bad fetch=%i expected=%i\n",(int)MYTHREAD,fetch, peer);
    abort();
  }
  upc_barrier;
  upc_all_free(sip);
  upc_barrier;
  if (!MYTHREAD) printf("test_upc: ending\n");
  return input;
}
