#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <upcxx/upcxx.hpp>

#include "test.h"

extern int test_upcxx(int input) {

  upcxx::init();

  if (!upcxx::rank_me()) printf("test_upcxx(%i): starting\n", input);
  upcxx::barrier();

  upcxx::global_ptr<int> gp = upcxx::new_<int>(upcxx::rank_me());
  upcxx::dist_object<upcxx::global_ptr<int>> dobj(gp);

  int peer = (upcxx::rank_me() + 1) % upcxx::rank_n();
  upcxx::global_ptr<int> pgp = dobj.fetch(peer).wait();
  int fetch = rget(pgp).wait();

  if (fetch != peer) {
    fprintf(stderr,"%i: ERROR bad fetch=%i expected=%i\n",(int)upcxx::rank_me(),fetch, peer);
    abort();
  }
  upcxx::barrier();

  char msg[80];
  snprintf(msg,sizeof(msg),"rank %i/%i: local rank %i/%i\n", upcxx::rank_me(), upcxx::rank_n(),
    upcxx::local_team().rank_me(), upcxx::local_team().rank_n());
  std::cout << msg << std::flush;

  int lpeer = ( upcxx::local_team().rank_me() + 1 ) % upcxx::local_team().rank_n();

  upcxx::dist_object<upcxx::global_ptr<int>> dobj_local(upcxx::local_team(), gp);
  upcxx::global_ptr<int> lgp = dobj_local.fetch(lpeer).wait();
  if (!lgp.is_local()) {
    std::cerr << upcxx::rank_me() << ": failed locality check for " << lgp << std::endl;
    abort();
  }
  int *llp = lgp.local();
  int expect = upcxx::local_team()[lpeer];
  int where = lgp.where();
  if (where != expect) {
    std::cerr << upcxx::rank_me() << "lgp:" << lgp << " .where()=" << where << " expect=" << expect << std::endl;
    abort();
  }
  int lfetch = *llp;
  if (lfetch != expect) {
    fprintf(stderr,"%i: ERROR bad fetch=%i expected=%i\n",(int)upcxx::rank_me(),lfetch, expect);
    abort();
  }

  upcxx::global_ptr<int> cgp = upcxx::try_global_ptr(llp);
  if (cgp != lgp) {
    std::cerr << upcxx::rank_me() << ": cgp:" << cgp << " != lgp:" << lgp << std::endl;
    abort();
  }

  upcxx::barrier();

  upcxx::delete_(gp);

  upcxx::barrier();
  if (!upcxx::rank_me()) std::cout << "test_upcxx: ending" << std::endl;

  upcxx::finalize();

  return input;
}
