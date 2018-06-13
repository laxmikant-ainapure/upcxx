/* This benchmark attempts to capture the overhead UPC++ adds on top of GASNet.
 * We  flood the wire with PUTs for sizes spread out over powers of 2.
 * We do this with upcxx using futures and promises, and gasnet with NB events.
 * 
 * Reported dimensions:
 * 
 *   peer = {self|local|remote}: Whether the destination rank was this rank, a
 *     rank local memory sharing, or remote rank. A single rank job will only
 *     emit self-puts. A multi-rank job will detect which of {local|remote} are
 *     available and will do one or both if possible.
 * 
 *   size: the size of the PUT in bytes.
 * 
 *   op = {put_lat|put_bw}:
 *     put_lat: Latency sensitive test where only one put is in-flight at
 *              a time.
 *     put_bw:  Bandwidth test which has lots of puts in flight concurrently.
 *
 *   how = {upcxx|gasnet}: Was this done using upcxx or gasnet API's.
 * 
 * Note that the intent of "how=gasnet" is not to benchmark gasnet's maximum
 * throughput, but to capture what the best possible implementation of the
 * corresponding "how=upcxx" could be expected to achieve.
 * 
 * Reported measurements:
 * 
 *   bw = Bandwidth in bytes/second. This is the measured quantity as dependent
 *     on the other dimensions. Even the "op=put_lat" present their throughput
 *     measurement as bandwidth.
 * 
 * Compile-time parameters (like -Dfoo=bar):
 *   
 *   PROGRESS_PERIOD=<int>: The number of PUTs to issue between calls to 
 *     `upcxx::progress()`. Default=32. Consider using a power of two so the
 *     modulo test is fast. Nobs will read this out of the "progress_period"
 *     environment variable.
 * 
 *   Also see those of: ./common/operator_new.hpp
 * 
 * Environment variables:
 * 
 *   put_size_ub: The maximum (inclusive) put size to capture in bytes. Only
 *     powers of two sizes up to and including this quantity are considered.
 *     Default = 4194304 (4MB)
 * 
 *   wait_secs: The number of (fractional) seconds to spend on each measurement.
 *     Default = 0.5. Larger values smooth out system noise.
 */

#include <upcxx/upcxx.hpp>
#include <upcxx/backend/gasnet/runtime_internal.hpp>

#include <gasnet.h>

#include "common/timer.hpp"
#include "common/report.hpp"
#include "common/operator_new.hpp"
#include "common/os_env.hpp"

#include <cstdint>
#include <string>
#include <iostream>
#include <list>
#include <vector>

#include <cstdio>

using namespace bench;
using namespace std;

#define USE_GPROF 0

constexpr bool DO_BANDWIDTH = true;
constexpr bool DO_UPCXX = true;
constexpr bool DO_GASNET = true;

#if USE_GPROF
  #include <sys/gmon.h>
  extern char __executable_start;
  extern char __etext;
#endif

#ifndef PROGRESS_PERIOD
  #define PROGRESS_PERIOD 32
#endif

struct measure {
  size_t ops;
  double secs;
  
  double bw(size_t op_size) const {
    return double(ops)*double(op_size)/secs;
  }
};

int main() {
  upcxx::init();
  
  #if USE_GPROF
    setenv("GMON_OUT_PREFIX", (std::string("gmon.rank-") + std::to_string(upcxx::rank_me())).c_str(), 1);
    monstartup(
      reinterpret_cast<u_long>(&__executable_start),
      reinterpret_cast<u_long>(&__etext)
    );
  #endif
  
  size_t size_ub = os_env<size_t>("put_size_ub", 4<<20);
  double wait_secs = os_env<double>("wait_secs", 0.5);
  
  constexpr int mag_lb = 3;
  constexpr int mag_ub_max = 30;
  
  int mag_ub = mag_lb;
  while(size_t(1)<<mag_ub <= size_ub)
    mag_ub += 1;
  
  UPCXX_ASSERT_ALWAYS(mag_ub < mag_ub_max, "put_size_ub="<<size_ub<<" cannot exceed "<<(size_t(1)<<mag_ub_max));
    
  upcxx::global_ptr<char> blob = upcxx::new_array<char>(size_ub);
  char *blob_local = blob.local();
  
  std::vector<upcxx::global_ptr<char>> blobs;
  blobs.resize(upcxx::rank_n());
  for(int i=0; i < upcxx::rank_n(); i++)
    blobs[i] = upcxx::broadcast(blob, i).wait(); 
  
  timer tim;
  
  if(upcxx::rank_me() == 0) {
    measure put_upcxx_lat[2][mag_ub_max+1];
    measure put_gas_lat[2][mag_ub_max+1];
    measure put_upcxx_bw_fut[2][mag_ub_max+1];
    measure put_upcxx_bw_pro[2][mag_ub_max+1];
    measure put_gas_bw[2][mag_ub_max+1];
    
    int local_peer = -1;
    int remote_peer = -1;
    for(int r=1; r < upcxx::rank_n(); r++) {
      if(!upcxx::local_team_contains(r)) {
        if(remote_peer == -1)
          remote_peer = r;
      }
      else {
        if(local_peer == -1)
          local_peer = r;
      }
    }
    
    int peers[2];
    int peer_n = 0;
    
    if(local_peer != -1) peers[peer_n++] = local_peer;
    if(remote_peer != -1) peers[peer_n++] = remote_peer;
    
    if(peer_n == 0) {
      cout<<"Running self-sends only.\n";
      peers[peer_n++] = 0;
    }
    else {
      cout<<"Running with peers: "
          <<"local="<<local_peer<<" and "
          <<"remote="<<remote_peer<<'\n';
    }
    
    for(int peer_ix=0; peer_ix < peer_n; peer_ix++) {
      const int peer = peers[peer_ix];
      cout<<"Putting to peer="<<peer<<'\n';
      
      for(int mag=mag_lb; mag < mag_ub; mag++) {
        const size_t size = 1<<mag;
        const int step = mag < 10 ? 1000 : mag < 20 ? 100 : 1<<(mag_ub-mag);
        
        if(DO_UPCXX) { // put upcxx latency
          cout<<"Measuring size="<<size<<" kind=lat how=upcxx"<<std::endl;
          cout.flush();
          
          tim.reset();
          int iters = 0;
          do {
            for(int i=0; i < step; i++) {
              auto src = blob_local;
              auto dest = blobs[peer];
              upcxx::rput(src, dest, size).wait();
            }
            iters += step;
          } while(tim.elapsed() < wait_secs);

          put_upcxx_lat[peer_ix][mag].ops = iters;
          put_upcxx_lat[peer_ix][mag].secs = tim.reset();
        }    
        
        if(DO_GASNET) { // put gasnet latency
          cout<<"Measuring size="<<size<<" kind=lat how=gasnet"<<std::endl;
          cout.flush();
          
          tim.reset();
          int iters = 0;
          do {
            for(int i=0; i < step; i++) {
              auto src = blob_local;
              auto dest = blobs[peer];
              gex_Event_t *e = new gex_Event_t(
                gex_RMA_PutNB(
                  upcxx::backend::gasnet::world_team, dest.rank_,
                  dest.raw_ptr_, (void*)src, size,
                  GEX_EVENT_DEFER,
                  /*flags*/0
                )
              );
              do gasnet_AMPoll();
              while(0 != gex_Event_Test(*e));
              delete e;
            }
            iters += step;
          } while(tim.elapsed() < wait_secs);
          
          put_gas_lat[peer_ix][mag].ops = iters;
          put_gas_lat[peer_ix][mag].secs = tim.reset();
        }
        
        if(DO_BANDWIDTH) { // put upcxx bandwidth over promises
          cout<<"Measuring size="<<size<<" kind=bw how=upcxx::pro"<<std::endl;
          cout.flush();
          
          tim.reset();
          int iters = 0;
          do {
            upcxx::promise<> pro;
            for(int i=0; i < step; i++) {
              auto src = blob_local;
              auto dest = blobs[peer];
              upcxx::rput(src, dest, size,
                upcxx::operation_cx::as_promise(pro)
              );
              if(0 == i%PROGRESS_PERIOD) upcxx::progress();
            }
            pro.finalize().wait();
            iters += step;
          } while(tim.elapsed() < wait_secs);
          
          put_upcxx_bw_pro[peer_ix][mag].ops = iters;
          put_upcxx_bw_pro[peer_ix][mag].secs = tim.reset();
        }
        
        if(DO_BANDWIDTH) { // put upcxx bandwidth over futures
          cout<<"Measuring size="<<size<<" kind=bw how=upcxx::fut"<<std::endl;
          cout.flush();
          
          tim.reset();
          int  iters = 0;
          do {
            upcxx::future<> f = upcxx::make_future();
            for(int i=0; i < step; i++) {
              auto src = blob_local;
              auto dest = blobs[peer];
              f = upcxx::when_all(f,
                upcxx::rput(src, dest, size)
              );
              if(0 == i%PROGRESS_PERIOD) upcxx::progress();
            }
            f.wait();
            iters += step;
          } while(tim.elapsed() < wait_secs);
          
          put_upcxx_bw_fut[peer_ix][mag].ops = iters;
          put_upcxx_bw_fut[peer_ix][mag].secs = tim.reset();
        }
        
        if(DO_BANDWIDTH) { // put gasnet bandwidth
          cout<<"Measuring size="<<size<<" kind=bw how=gasnet"<<std::endl;
          cout.flush();
          
          tim.reset();
          int iters = 0;
          do {
            std::list<gex_Event_t> evs;
            
            for(int i=0; i < step; i++) {
              auto src = blob_local;
              auto dest = blobs[peer];
              
              gex_Event_t h = gex_RMA_PutNB(
                upcxx::backend::gasnet::world_team, dest.rank_,
                dest.raw_ptr_, (void*)src, size,
                GEX_EVENT_DEFER,
                /*flags*/0
              );
              
              if(gex_Event_Test(h) != 0) {
                evs.push_back(h);
                if(0 == i%32) gasnet_AMPoll();
                { // after_gasnet
                  auto it = evs.begin();
                  int n = 0;
                  while(it != evs.end() && n < 4) {
                    if(gex_Event_Test(*it) == 0)
                      evs.erase(it++);
                    else
                      { ++it; ++n; }
                  }
                }
              }
            }
            
            while(!evs.empty()) {
              gasnet_AMPoll();
              while(!evs.empty() && 0 == gex_Event_Test(evs.front()))
                evs.pop_front();
            }
            
            iters += step;
          } while(tim.elapsed() < wait_secs);
          
          put_gas_bw[peer_ix][mag].ops = iters;
          put_gas_bw[peer_ix][mag].secs = tim.reset();
        }
      }
    }
    
    report rep(__FILE__);
    
    for(int peer_ix=0; peer_ix < peer_n; peer_ix++) {
      const char *peer = 
        peers[peer_ix]==local_peer ? "local" :
        peers[peer_ix]==remote_peer ? "remote" :
        "self";
      
      for(int mag=mag_lb; mag < mag_ub; mag++) {
        const int size = 1<<mag;
        auto common = opnew_row()
                    & column("peer", peer)
                    & column("size", size)
                    & column("progress_period", PROGRESS_PERIOD);
        
        if(DO_UPCXX) {
          rep.emit({"bw"},
            common &
            column("op", "put_lat") &
            column("how", "upcxx") &
            column("bw", put_upcxx_lat[peer_ix][mag].bw(size))
          );
        }
        if(DO_GASNET) {
          rep.emit({"bw"},
            common &
            column("op", "put_lat") &
            column("how", "gasnet") &
            column("bw", put_gas_lat[peer_ix][mag].bw(size))
          );
        }
        if(DO_BANDWIDTH) {
          rep.emit({"bw"},
            common &
            column("op", "put_bw") &
            column("how", "upcxx_fut") &
            column("bw", put_upcxx_bw_fut[peer_ix][mag].bw(size))
          );
          rep.emit({"bw"},
            common &
            column("op", "put_bw") &
            column("how", "upcxx_pro") &
            column("bw", put_upcxx_bw_pro[peer_ix][mag].bw(size))
          );
          rep.emit({"bw"},
            common &
            column("op", "put_bw") &
            column("how", "gasnet") &
            column("bw", put_gas_bw[peer_ix][mag].bw(size))
          );
        }
        
        rep.blank();
      }
    }
  }
  
  upcxx::barrier();
  
  #if USE_GPROF
    _mcleanup();
    fflush(0);
  #endif
  
  upcxx::finalize();
}
