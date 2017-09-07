// Fill the ghost cells, 1D Red Black Gauss Seidel

#include <assert.h>
#include <stdlib.h>
#include <iostream>
#include <upcxx/upcxx.hpp>
using namespace std;
using namespace upcxx;

void fillGhost(double *u, int hi, int lo, global_ptr<double> &uL, global_ptr<double> &uR)
{
  intrank_t myrank = upcxx::rank_me();
  intrank_t nranks = upcxx::rank_n();
  future<double> fL, fR;
  if ( !(uL.is_null())){
      fL = rget(uL);                                    // Equivalent to future<> fL = rget(uL,&u0,1)
//      cout << "Left Get on " << myrank << " initiates\n" ;
      double u0 = wait(fL);
      cout << "Got " << u0 << " from Left\n";
//      cout << "Left Get on " << myrank << " completes\n" ;
      u[0] = u0;
  }
  if ( !(uR.is_null())){
      fR = rget(uR);
//      cout << "Right Get on " << myrank << " initiates\n" ;
      double u1 = wait(fR);
      cout << "Got " << u1 << " from Right\n";
//      cout << "Right Get on " << myrank << " completes\n" ;
      u[hi-lo] = u1;
  }
}
