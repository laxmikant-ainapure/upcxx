## Recommendations for hybrid UPC++/MPI applications

With care, it is possible to construct applications which use both UPC++ and MPI
explicitly in user code (note that this is different from UPC++ programs which
do not explicitly use MPI, but are compiled with 'CONDUIT=mpi': such programs do
not require any special treatment). Note however, that strict programming
conventions (below) must be adhered to when switching between MPI and UPC++
network communication, otherwise deadlock can result on many systems.

In general, mixed MPI/UPC++ applications must be linked with an MPI C++
compiler.  This may be named 'mpicxx' or 'mpic++', among other possible names.
However, on Cray systems 'CC' is both the regular C++ compiler and the MPI C++
compiler.  You may need to pass this same compiler as $CXX when installing UPC++
to ensure object compatibility.

Certain UPC++ network types (currently 'mpi' and 'ibv') may use MPI
internally. For this reason, MPI objects should be compiled with the same MPI
compiler that was used when UPC++ itself was build (normally the 'mpicc' in
one's $PATH, unless some action is taken to override that default).
Additionally, the MPI portion of an application should make use of
'MPI_Initialized()' to ensure exactly one call is made to initialize MPI.

Both MPI and UPC++ cause network communication, and the respective runtimes do
so without any coordination. As a result, it is quite easy to cause network
deadlock when mixing MPI and UPC++, unless the following protocol is strictly
observed:

1.  When the application starts, the first MPI or UPC++ call (*i.e.*
    'MPI_Init()' or 'upcxx::init()') which may result in network traffic from
    any thread should be considered to put the entire job in 'MPI' or 'UPC++'
    mode, respectively.

2.  When an application is in 'MPI' mode, and needs to switch to using UPC++, it
    should quiesce all MPI operations in-flight and then collectively execute an
    'MPI_Barrier()' as the last MPI call before causing any UPC++
    communication. Once any UPC++ communication has occurred from any rank, the
    program should be considered to have switched to 'UPC++' mode.

3.  When an application is in 'UPC++' mode, and an MPI call that may communicate
    is needed, the application must quiesce all UPC++ communication and then
    execute a upcxx::barrier() before any MPI calls are made. Once any MPI
    functions have been called from any thread, the program should be considered
    to be in 'MPI' mode.

If this simple construct of alternating MPI and UPC++ phases can be observed,
then it should be possible to avoid deadlock.


### Troubleshooting:

If your application follows the recommendation above but still has problems,
here are some things to consider:

1.  Some HPC networks APIs are not well-suited to running multiple runtimes.
    They may either permit a process to "open" the network adapter at most once,
    or they may provide multiple opens but allow them to block one another.
    Additionally, it is possible in some cases that one network runtime may
    monopolize the network resources, leading to abnormally slow performance or
    complete failure of the other.  The solutions available for these case are
    either to use distinct networks for UPC++ and MPI, or to use mpi-conduit for
    UPC++.  These have adverse performance impact on different portions of your
    code, and the best choice will depend on specifics of your code.
    
    1.  Use TCP-based communication in MPI  
	      We have compiled a list of the relevant configuration parameters for
        several common MPI implementations, which can be found
        [here](http://gasnet.lbl.gov/dist/other/mpi-spawner/README) (look for
        the phrase "hybrid GASNet+MPI").
    2.  Use UDP for communication in UPC++  
        Set `CONDUIT=udp` when installing UPC++
    3.  Use MPI for communication in UPC++  
        Set `CONDUIT=mpi` when installing UPC++

2.  The Aries network adapter on the Cray XC platform has approximately 120
    hardware contexts for communications.  With MPI and UPC++ each consuming one
    per process, a 64-process-per-node run of a hybrid application exceeds the
    available resources.  The solution is to set the following two environment
    variables at run time to instruct both libraries to request virtualized
    contexts: ``` GASNET_GNI_FMA_SHARING=1 MPICH_GNI_FMA_SHARING=enabled ```
