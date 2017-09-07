#include <assert.h>
#include <stdlib.h>
#include <iostream>
#include <math.h>
#include <upcxx/upcxx.hpp>

using namespace std;
using namespace upcxx;


void  dumpAns(double *u, int lo, int hi, int N){
   /* Optional dump of final answer... omit if not interested.
    * This has the processors output their chunks in order...
    * by waiting for and passing along a "my turn" token.
    * This is a luxury we can afford since we're
    * no longer in the timed section of the code.
    *
    * Output the computed and exact solution
    *  if there are only a small number of points
    */

   intrank_t myrank = upcxx::rank_me();
   intrank_t nranks = upcxx::rank_n();
   double h = 1.0 / (N-1.0);
   if ((N-2) <= 16) {
     int a, i;
      if (nranks > 1) {
#if 0
	 MPI_Status stat;
         if ( !myrank )
            MPI_Send(&a, 1, MPI_INT, myrank+1, ANSWR_TYPE, MPI_COMM_WORLD);
         else {
            MPI_Recv(&a, 1, MPI_INT, MPI_ANY_SOURCE,
				 ANSWR_TYPE, MPI_COMM_WORLD, &stat);
         }
#else
// *** Need to write
#endif
      }

      for (i = (lo==0)?lo:lo+1; i <= (hi==(N-1)?hi:hi-1); i++) {
        printf("u[%d]:  %g (%g)\n", i, u[i-lo],4.0e0*i*h*(i*h-1.0e0));
      }

      if ((nranks > 1) && (myrank != nranks-1)) {
#if 0
	 MPI_Send(&a, 1, MPI_INT, myrank+1, ANSWR_TYPE, MPI_COMM_WORLD);
#else
// *** Need to write
#endif
      }
   }
}
