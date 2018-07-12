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
 *   kind = {lat|bw}:
 *     lat: Latency sensitive test where only one put is in-flight at a time.
 *     bw:  Bandwidth test which has lots of puts in flight concurrently.
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
 *   sizes: The list of transfer sizes to measure in bytes. 
 *     Default = 8...4M
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
#include <unordered_map>
#include <vector>

#include <cstdio>

using namespace bench;
using namespace std;

#define USE_GPROF 0

#if USE_GPROF
  #include <sys/gmon.h>
  extern char __executable_start;
  extern char __etext;
#endif

#ifndef PROGRESS_PERIOD
  #define PROGRESS_PERIOD 32
#endif

int main() {
  upcxx::init();
  
  #if USE_GPROF
    setenv("GMON_OUT_PREFIX", (std::string("gmon.rank-") + std::to_string(upcxx::rank_me())).c_str(), 1);
    monstartup(
      reinterpret_cast<u_long>(&__executable_start),
      reinterpret_cast<u_long>(&__etext)
    );
  #endif
  
  vector<size_t> sizes =
    os_env<vector<size_t>>("sizes",
      {8, 16, 32, 64, 128, 256, 512,
       1<<10, 2<<10, 4<<10, 8<<10, 16<<10, 32<<10, 64<<10,
       128<<10, 256<<10, 512<<10, 1<<20, 2<<20, 4<<20}
    ); 
  double wait_secs = os_env<double>("wait_secs", 0.5);
  
  size_t max_size = 0;
  for(size_t sz: sizes)
    max_size = std::max(max_size, sz);
  
  UPCXX_ASSERT_ALWAYS(max_size <= 1u<<30, "max size in `sizes` cannot exceed "<<(1u<<30));
    
  upcxx::global_ptr<char> blob = upcxx::new_array<char>(2*max_size);
  char *blob_local = blob.local();
  
  std::vector<upcxx::global_ptr<char>> blobs;
  blobs.resize(upcxx::rank_n());
  for(int i=0; i < upcxx::rank_n(); i++)
    blobs[i] = upcxx::broadcast(blob, i).wait(); 
  
  if(upcxx::rank_me() == 0) {
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
    
    auto make_row =
      [&](int peer, size_t size, const char *kind, const char *how) {
        return column("peer",
                 peer == local_peer ? "local" :
                 peer == remote_peer ? "remote" :
                 "self"
               ) &
               column("size", size) &
               column("kind", kind) &
               column("how", how);
      };
    
    using row_t = decltype(make_row(0,0,0,0));
    
    std::unordered_map<row_t, double> bw_table;
    
    for(int peer_ix=0; peer_ix < peer_n; peer_ix++) {
      const int peer = peers[peer_ix];
      cout<<"Putting to peer="<<peer<<'\n';
      
      for(size_t size: sizes) {
        const int step = (1<<20)/size | 1;
        
        if(1) { // put upcxx latency
          cout<<"Measuring size="<<size<<" kind=lat how=upcxx"<<std::endl;
          cout.flush();
          
          timer tim;
          int64_t ops = 0;
          do {
            for(int i=0; i < step; i++) {
              auto src = blob_local;
              auto dest = blobs[peer] + size;
              upcxx::rput(src, dest, size).wait();
            }
            ops += step;
          } while(tim.elapsed() < wait_secs);
          
          bw_table[make_row(peer, size, "lat", "upcxx")] = ops*size/tim.elapsed();
        }
        
        if(1) { // put gasnet latency
          cout<<"Measuring size="<<size<<" kind=lat how=gasnet"<<std::endl;
          cout.flush();
          
          timer tim;
          int64_t ops = 0;
          do {
            for(int i=0; i < step; i++) {
              auto src = blob_local;
              auto dest = blobs[peer] + size;
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
            ops += step;
          } while(tim.elapsed() < wait_secs);
          
          bw_table[make_row(peer, size, "lat", "gasnet")] = ops*size/tim.elapsed();
        }
        
        if(1) { // put upcxx bandwidth over promises
          cout<<"Measuring size="<<size<<" kind=bw how=upcxx-pro"<<std::endl;
          cout.flush();
          
          timer tim;
          int64_t ops = 0;
          upcxx::promise<> pro;
          
          do {
            for(int i=0; i < step; i++) {
              auto src = blob_local;
              auto dest = blobs[peer] + size;
              upcxx::rput(src, dest, size,
                upcxx::operation_cx::as_promise(pro)
              );
              ops += 1;
              
              if(0 == ops%PROGRESS_PERIOD)
                upcxx::progress();
            }
          }
          while(tim.elapsed() < wait_secs);
          
          pro.finalize().wait();
          
          bw_table[make_row(peer, size, "bw", "upcxx-pro")] = ops*size/tim.elapsed();
        }
        
        if(1) { // put upcxx bandwidth over futures
          cout<<"Measuring size="<<size<<" kind=bw how=upcxx-fut"<<std::endl;
          cout.flush();
          
          timer tim;
          int64_t ops = 0;
          std::list<upcxx::future<>> futs;
          
          do {
            for(int i=0; i < step; i++) {
              auto src = blob_local;
              auto dest = blobs[peer] + size;
              futs.push_back(upcxx::rput(src, dest, size));
              ops += 1;
              
              if(0 == ops%PROGRESS_PERIOD) {
                upcxx::progress();
                
                while(!futs.empty() && futs.front().ready())
                  futs.pop_front();
              }
            }
          }
          while(tim.elapsed() < wait_secs);
          
          while(!futs.empty()) {
            futs.front().wait();
            futs.pop_front();
          }
          
          bw_table[make_row(peer, size, "bw", "upcxx-fut")] = ops*size/tim.elapsed();
        }
        
        if(1) { // put gasnet bandwidth
          cout<<"Measuring size="<<size<<" kind=bw how=gasnet"<<std::endl;
          cout.flush();
          
          timer tim;
          int64_t ops = 0;
          std::list<gex_Event_t> evs;
          
          do {
            for(int i=0; i < step; i++) {
              auto src = blob_local;
              auto dest = blobs[peer] + size;
              gex_Event_t e = gex_RMA_PutNB(
                upcxx::backend::gasnet::world_team, dest.rank_,
                dest.raw_ptr_, (void*)src, size,
                GEX_EVENT_DEFER,
                /*flags*/0
              );
              ops += 1;
              
              if(0 != gex_Event_Test(e))
                evs.push_back(e);
              
              if(0 == ops%PROGRESS_PERIOD) {
                gasnet_AMPoll();
                while(!evs.empty() && 0 == gex_Event_Test(evs.front()))
                  evs.pop_front();
              }
            }
          }
          while(tim.elapsed() < wait_secs);
          
          while(!evs.empty()) {
            gasnet_AMPoll();
            while(!evs.empty() && 0 == gex_Event_Test(evs.front()))
              evs.pop_front();
          }

          bw_table[make_row(peer, size, "bw", "gasnet")] = ops*size/tim.elapsed();
        }
        
        if(1) { // put gasnet-nbi bandwidth
          cout<<"Measuring size="<<size<<" kind=bw how=gasnet-nbi"<<std::endl;
          cout.flush();
          
          timer tim;
          int64_t ops = 0;
          
          gex_NBI_BeginAccessRegion(0);
          do {
            for(int i=0; i < step; i++) {
              auto src = blob_local;
              auto dest = blobs[peer] + size;
              gex_RMA_PutNBI(
                upcxx::backend::gasnet::world_team, dest.rank_,
                dest.raw_ptr_, (void*)src, size,
                GEX_EVENT_DEFER,
                /*flags*/0
              );
              ops += 1;
            }
          }
          while(tim.elapsed() < wait_secs);
          
          gex_Event_t e = gex_NBI_EndAccessRegion(0);
          gex_Event_Wait(e);
          
          bw_table[make_row(peer, size, "bw", "gasnet-nbi")] = ops*size/tim.elapsed();
        }
      }
    }
    
    report rep(__FILE__);
    
    for(int peer_ix=0; peer_ix < peer_n; peer_ix++) {
      for(size_t size: sizes) {
        const char *LAT = "lat", *BW = "bw";
        for(const char *kind: {LAT,BW}) {
          for(const char *how:
              kind == LAT
                ? std::initializer_list<const char*>{"upcxx","gasnet"}
                : std::initializer_list<const char*>{"upcxx-fut","upcxx-pro","gasnet","gasnet-nbi"}
            ) {
            
            auto r = make_row(peers[peer_ix], size, kind, how);
            
            if(bw_table.count(r)) { // in case one of the trials is disabled via "if(0) ..."
              rep.emit({"bw"},
                r & opnew_row()
                  & column("progress_period", PROGRESS_PERIOD)
                  & column("bw", bw_table[r])
              );
            }
          }
        }
        
        rep.blank();
      }
    }
  } // rank_me()==0
  
  upcxx::barrier();
  
  #if USE_GPROF
    _mcleanup();
    fflush(0);
  #endif
  
  upcxx::finalize();
}
