/*
 *  MPI code that implements Red Black Gauss Seidel method
 *  to solve an ODE in 1 dimension 
 *  This code was written by Scott B. Baden and Feng Tian
 *  Fillghost was recodd by Scott B. Baden, 2017-08-09
 *
*/

#include <assert.h>
#include <stdlib.h>
#include <iostream>
#include <math.h>
#include <mpi.h>
#include "rb1d.h"

using namespace std;

#define MAX(x,y) ((x) > (y) ? (x) : (y))

void  dumpAns(double *u, int lo, int hi, int N);

int main(int argc, char **argv)
{
   int N;                       // Size of the input
   bool printConvg;             // print convergence data
   bool noComm;                 // Shut off communication
   double epsilon; 		// Stopping value for error
   int freq = 1; 		// Convergence check frequency

   int i, s;
   int lo, hi, OE, MaxIter, LocPnts;
   int myrank, nprocs;

   double h;
   double *u, *uexact;
   double locErr, err;
   double t0, t1, tdiff;

   void fillGhost(double *u, int hi, int lo);
   void cmdLine(int argc, char *argv[], int& N, double& epsilon, int& chk_freq,
		int& MaxIter, bool& printConvg, bool& noComm);

   MPI_Init(&argc,&argv);
   MPI_Comm_rank(MPI_COMM_WORLD,&myrank);
   MPI_Comm_size(MPI_COMM_WORLD,&nprocs);


// Get the command line arguments
   cmdLine(argc, argv, N, epsilon, freq, MaxIter, printConvg, noComm);

   h = 1.0 / (N-1.0);

   if (!myrank ){
      printf("# points: %d\n", N);
      printf("H: %g\n", h);
      printf("Maximum Iterations: %d\n", MaxIter);
      printf("Check freq: %d\n", freq);
      printf("Convergence threshold: %g\n", epsilon);
      if (noComm)
	 printf("Communication SHUT OFF.\n");
   }
    
   //
   // Divide points among processors as evenly as possible
   //

   // lowest and highest panel numbers at this processor
   lo = myrank * (N/nprocs);
   if (myrank < N%nprocs)
     lo += myrank;
   else
     lo += N%nprocs;

   hi = lo + (N/nprocs) - 1;
   if (myrank < N%nprocs)
     hi++;

   if (hi < lo)
     printf("Warning: Processor %d is idle.\n", myrank);
   
   N += 2;      // Update size of array of points for boundaries

   /*
    * Make sure the extremities of the array have space
    * for a neighbor's value, except for the processors on the end
    *
    */
   if (myrank > 0)
      lo--;
   if (myrank < nprocs-1)
      hi++;
   
   //
   // Each now rank has points lo through hi

   LocPnts = hi - lo + 1;               // # local points

// We use this to get the odd/even business correct on multiple processors
   OE = lo % 2;


   u = (double*)malloc(sizeof(double)*LocPnts);
   uexact = (double*)malloc(sizeof(double)*LocPnts);
   /* uexact is the exact solution, used to compute error */

   // Initial guess is all 1's
   for (i = 1; i < LocPnts-1; i++) {
       u[i] = 1.0;
       double x = (lo+i)*h;
       uexact[i] = 4*x*(x-1.0);
   }
   
   // Set boundaries to 0
   u[0] = u[LocPnts-1] = 0.0;

   MPI_Barrier(MPI_COMM_WORLD);
   t0 = MPI_Wtime();
   for (s = 0; s < MaxIter; s++) {

       // Compute values of u for next time step
       locErr = err = 0.0;
       // Do the odd Phase
       /* Here is where we obtain values from neighboring processors
           * First the new u_lo boundary for this processor,
       * then the new u_hi boundary
       */
       if ((nprocs > 1) & (!noComm))
           fillGhost(u,lo,hi);

       for (i = OE+ 1; i < LocPnts-1; i+=2)
           u[i] = (u[i-1] + u[i+1] - 8.0*h*h)/2.0;

        // Do the Even Phase
        if ((nprocs > 1) & (!noComm))
           fillGhost(u,lo,hi);

        for (i = 2-OE; i < LocPnts-1; i+=2) 
            u[i] = (u[i-1] + u[i+1] - 8.0*h*h)/2.0;


        /* Check convergence if it's time
        * We compute the max norm error in the max norm:  we take the
        * pointwise differences between the computed solution u[]
        * and the exact solution uexact[].
        * The error is largest absolute value over all the differences
        *
        */
        if ( ( freq == 1 ) || (s == (MaxIter-1))  || ( ( s % freq ) == 0 ) ) {
            for (i = 1; i < LocPnts-1; i++) {
               double delta = fabs(uexact[i] - u[i]);
               locErr =  MAX(locErr,delta);
	}

	MPI_Allreduce ( &locErr, &err, 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);

	// Print out convergence information if it's time
	if (!myrank && printConvg ) 
	    printf("Error at Iteration %d = %g\n", s, err);
        if ( err <= epsilon )
	    break;
        }

   } // End of iteration loop. */
   MPI_Barrier(MPI_COMM_WORLD);
   
   t1  = MPI_Wtime();
   tdiff = t1 - t0;

   if (!myrank ) {
     printf("Ran on %d nprocs with %d updated points.\n", nprocs, N-2);
     if (err <= epsilon) 
	 printf("Converged after %d iterations with %g error.\n", s, err);
     else
	 printf("Did not converge.  Error after %d iterations: %g.\n", s, err);
     printf("Wall clock time:  %g\n", tdiff);
     printf("Grind time:  %g\n", tdiff/((N-2) * s));
   }

   if ((N-2) <= 16) 
       dumpAns(u,lo,hi,N);
  
   MPI_Finalize();
   return 0;
}
