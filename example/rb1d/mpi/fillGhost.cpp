// Fill the ghost cells, 1D Red Black Gauss Seidel

#include <assert.h>
#include <stdlib.h>
#include <iostream>
#include <mpi.h>
#include "rb1d.h"
using namespace std;

void fillGhost(double *u, int lo, int hi)
{
  MPI_Status inStat[2], outStat[2];
  MPI_Request outReq[2], inReq[2];
  int Tag1 = 1;  
  int myid, nodes;
  MPI_Comm_rank(MPI_COMM_WORLD,&myid);
  MPI_Comm_size(MPI_COMM_WORLD,&nodes);

  const int LEFT = 0, RIGHT = 1;

  int err0, err1;
  if (myid != 0){
     int err0 = MPI_Irecv(u+0,1,MPI_DOUBLE,myid-1, Tag1,MPI_COMM_WORLD,&inReq[LEFT]);
     assert (!err0);
  }
  if (myid != (nodes-1)){
     int err1 = MPI_Irecv(u+hi-lo,1,MPI_DOUBLE,myid+1, Tag1,MPI_COMM_WORLD,&inReq[RIGHT]);
     assert (!err1);
  }
  if (myid != (nodes-1)){ 			// Send right
     int errs0 = MPI_Isend(u+hi-lo-1,1,MPI_DOUBLE,myid+1,Tag1,MPI_COMM_WORLD,&outReq[RIGHT]);
     assert (!errs0);
  }
 
 if (myid != 0){                                // Send Left
     int errs1 = MPI_Isend(u+1,1,MPI_DOUBLE,myid-1,Tag1,MPI_COMM_WORLD,&outReq[LEFT]);
     assert (!errs1);
  }

 // Wait on isends and irecvs
  MPI_Status Sr[2], Ss[2];

  if (myid != 0){
     int err0 = MPI_Wait(&inReq[LEFT],&Sr[LEFT]);
     assert (!err0);
  }
  if (myid != (nodes-1)){
     int err1 = MPI_Wait(&inReq[RIGHT],&Sr[RIGHT]);
     assert (!err1);
  }
  if (myid != (nodes-1)){
     int errs0 = MPI_Wait(&outReq[RIGHT],&Ss[RIGHT]);
     assert (!errs0);
  }
  if (myid != 0){
     int errs1 = MPI_Wait(&outReq[LEFT],&Ss[LEFT]);
     assert (!errs1);
  }
}
