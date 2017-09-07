#include <assert.h>
#include <stdlib.h>
#include <iostream>
#include <math.h>
#include <mpi.h>
#include "rb1d.h"

using namespace std;


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

   int myrank;
   int nprocs;
   MPI_Comm_rank(MPI_COMM_WORLD,&myrank);
   MPI_Comm_size(MPI_COMM_WORLD,&nprocs);
   double h = 1.0 / (N-1.0);
   if ((N-2) <= 16) {
     int a, i;
      if (nprocs > 1) {
	 MPI_Status stat;
         if ( !myrank )
            MPI_Send(&a, 1, MPI_INT, myrank+1, ANSWR_TYPE, MPI_COMM_WORLD);
         else {
            MPI_Recv(&a, 1, MPI_INT, MPI_ANY_SOURCE,
				 ANSWR_TYPE, MPI_COMM_WORLD, &stat);
         }
      }

      for (i = (lo==0)?lo:lo+1; i <= (hi==(N-1)?hi:hi-1); i++) {
        printf("u[%d]:  %g (%g)\n", i, u[i-lo],4.0e0*i*h*(i*h-1.0e0));
      }

      if ((nprocs > 1) && (myrank != nprocs-1)) {
	 MPI_Send(&a, 1, MPI_INT, myrank+1, ANSWR_TYPE, MPI_COMM_WORLD);
      }
   }
}
