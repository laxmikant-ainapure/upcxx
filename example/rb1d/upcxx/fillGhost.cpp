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
  if ( !(uL.is_null())){
      // Equivalent to future<> fL = rget(uL,&u0,1)
      
      printf("Left ptr: %p [%d]\n",uL,myrank);
      future<double> fL = rget(uL+hi-lo-1);  
      double u0 = wait(fL);
//      cout << "Got " << u0 << " from Left\n";
      u[0] = u0;
  }
  if ( !(uR.is_null())){
      future<double> fR = rget(uR+1);
      printf("Right ptr: %p [%d]\n",uR,myrank);
      double u1 = wait(fR);
//      cout << "Got " << u1 << " from Right\n";
      u[hi-lo] = u1;
  }
}
