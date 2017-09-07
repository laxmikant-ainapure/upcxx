/*
 *  UPC++ code that implements Red Black Gauss Seidel method
 *  to solve an ODE in 1 dimension 
 *  This code was first written in MPI by Scott B. Baden and Feng Tian
 *  Fillghost was recoded by Scott B. Baden, 2017-08-09
 *  Converted to UPC++ by Scott Baden, 2017-09-02
 *
*/

#include <upcxx/upcxx.hpp>
#include <stdlib.h>
#include <iostream>

using namespace std;
using namespace upcxx;          // So if writing a .cpp file, using the name space is OK
                                // But we don't do this inside headers, to
                                // avoid possible namespace conflicts

void  dumpAns(double *u, int lo, int hi, int N);
double getTime();

template <typename T>
upcxx::future<T> fetch(upcxx::dist_object<T> &dobj, upcxx::intrank_t rank) {
   return upcxx::rpc(rank, [](upcxx::dist_object<T> &rdobj) { return *rdobj; }, dobj);
}

int main(int argc, char **argv)
{
   int N;                       // Size of the input
   bool printConvg;             // print convergence data
   bool noComm;                 // Shut off communication
   double epsilon; 		// Stopping value for error
   int freq = 1; 		// Convergence check frequency

   int i, s;
   int lo, hi, OE, MaxIter, LocPnts;

   double h;

   void fillGhost(double *u, int hi, int lo, global_ptr<double> &uL, global_ptr<double> &uR);
   void cmdLine(int argc, char *argv[], int& N, double& epsilon, int& chk_freq,
		int& MaxIter, bool& printConvg, bool& noComm);

   upcxx::init();
   // Why was intrank_t causing compiler errors
   intrank_t myrank = upcxx::rank_me();
   intrank_t nranks = upcxx::rank_n();

//   MPI_Comm_rank(MPI_COMM_WORLD,&myrank);
//   MPI_Comm_size(MPI_COMM_WORLD,&nranks);


// Get the command line arguments
   cmdLine(argc, argv, N, epsilon, freq, MaxIter, printConvg, noComm);

   h = 1.0 / (N-1.0);

   if (!myrank ){
      printf("N: %d\n", N);
      printf("H: %g\n", h);
      printf("Maximum Iterations: %d\n", MaxIter);
      printf("Convergence check freq: %d\n", freq);
      printf("Convergence threshold: %g\n", epsilon);
      if (noComm)
	 printf("Communication SHUT OFF.\n");
   }
    
   //
   // Divide points among processors as evenly as possible
   //

   // lowest and highest panel numbers at this processor
   lo = myrank * (N/nranks);
   if (myrank < N%nranks)
     lo += myrank;
   else
     lo += N%nranks;

   hi = lo + (N/nranks) - 1;
   if (myrank < N%nranks)
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
   if (myrank < nranks-1)
      hi++;
   
   //
   // Each now rank has points lo through hi

   LocPnts = hi - lo + 1;               // # local points

// We use this to get the odd/even business correct on multiple processors
   OE = lo % 2;


//   u = (double*)malloc(sizeof(double)*LocPnts);
   double *uexact = (double*)malloc(sizeof(double)*LocPnts);
   /* uexact is the exact solution, used to compute error */


//   int LeftN =  (myrank != 0) ?  myrank-1 : -1;
//   int RightN = (myrank != (nranks-1)) ?  myrank+1 : -1;
   global_ptr<double> U  = new_array<double>(LocPnts);
   upcxx::dist_object<global_ptr<double>> UU(U);                // Equivalent to *UU=U;


   // Swap pointers to get Left and Right
   // Left and right pointers are NULL where there is a refernce beyond the
   // physical boundary
   global_ptr<double> uL(nullptr), uR(nullptr);
   // No barrier needed to get quiescence on (collective) dist_obj construction,
   // since the fetch does an RPC that handles the required synchronization
   if (myrank != 0){
       upcxx::future<global_ptr<double>> fL = fetch(UU,myrank-1);
       uL = upcxx::wait(fL);
   }
   if (myrank != (nranks-1)){
       upcxx::future<global_ptr<double>> fR = fetch(UU,myrank+1);
       uR = upcxx::wait(fR);
   }

   upcxx::barrier();

   // Get the local pointer
   // We must do this because we cannot dereference a global pointer
   assert(U.is_local());
   double *u = U.local();                                       // not defined unless is_local() returns true

   printf("local global segment on rank %d: %x\n",myrank,U.local());
   
   // Initial guess is all 1's
   for (i = 1; i < LocPnts-1; i++) {
       u[i] = 1.0;
       double x = (lo+i)*h;
       uexact[i] = 4*x*(x-1.0);
   }
   
  // Set boundaries to 0
   u[0] = u[LocPnts-1] = 0.0;

   barrier();
   double t0 = -getTime();
// ** Take time ** //
   double maxErr = 0.0, locErr = 0.0;
   for (s = 0; s < MaxIter; s++) {

       // Compute values of u for next time step
       // Do the odd Phase
       /* Here is where we obtain values from neighboring processors
           * First the new u_lo boundary for this processor,
       * then the new u_hi boundary
       */
       if ((nranks > 1) & (!noComm))
           fillGhost(u,lo,hi,uL,uR);

       for (i = OE+ 1; i < LocPnts-1; i+=2)
           u[i] = (u[i-1] + u[i+1] - 8.0*h*h)/2.0;

       barrier();                                    // Can't start fillGhost until the values have been computed
       // Do the Even Phase
       if ((nranks > 1) & (!noComm))
           fillGhost(u,lo,hi,uL, uR);

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
            locErr = 0.0;
            for (i = 1; i < LocPnts-1; i++) {
               double delta = fabs(uexact[i] - u[i]);
               locErr =  std::max(locErr,delta);
            }

            maxErr = wait(allreduce(locErr,  [](const double & a, const double & b){ return std::max(a,b);}));

	    // Print out convergence information if it's time
	    if (!myrank && printConvg ) 
	        printf("Error at Iteration %d = %g\n", s, maxErr);
            if ( maxErr <= epsilon )
	        break;
        }

        barrier();

   } // End of iteration loop. */
   barrier();
   
   t0 += getTime();
// ** Take time ** //

   if (!myrank ) {
     printf("Ran on %d nranks with %d updated points.\n", nranks, N-2);
     if (maxErr <= epsilon) 
	 printf("Converged after %d iterations with %g error.\n", s, maxErr);
        else
            printf("Did not converge.  L2 Error after %d iterations: %g.\n", s, maxErr);
     printf("Wall clock time:  %g s.\n", t0);
     printf("Grind time:  %g s.\n", t0/((N-2) * s));
   }

   if ((N-2) <= 16) 
       dumpAns(u,lo,hi,N);
  
   barrier();        // ensures dist_object lifetime
   upcxx::finalize();

   return 0;
}
