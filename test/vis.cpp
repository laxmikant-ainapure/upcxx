#include <cstdint>
#include <vector>
#include <cstdlib>
#include <cstddef>
#include <iterator>

#include <upcxx/diagnostic.hpp>
#include <upcxx/allocate.hpp>
#include <upcxx/future.hpp>
#include <upcxx/wait.hpp>
#include <upcxx/vis.hpp>
#include <upcxx/dist_object.hpp>

#include "util.hpp"

using namespace upcxx;
using namespace std;

#define M 68
#define N 128
#define B 20

typedef long long int lli;

typedef lli patch_t[M][N];

template<typename ptr_t>
class Iter 
{
public:
  Iter() = default;
  Iter(ptr_t a_p,  std::size_t a_size, int a_stride)
    :m_ptr(a_p),m_size(a_size), m_stride(a_stride) {}

  
  bool operator==(Iter rhs) const { return rhs.m_ptr==m_ptr;}
  bool operator!=(Iter rhs) const { return rhs.m_ptr!=m_ptr;}
  Iter& operator++(){m_ptr+=m_stride; return *this;}
  Iter  operator++(int){Iter rtn(*this); m_ptr+=m_stride; return rtn;} 

  void operator+=(std::ptrdiff_t a_skip) {m_ptr = m_ptr+a_skip;}

  std::ptrdiff_t operator-(const Iter& rhs) const {
    assert(m_stride==rhs.m_stride);
    return (m_ptr - rhs.m_ptr)/m_stride;
  }
  ptr_t m_ptr=0;
  std::size_t m_size=0;
  int   m_stride=0;
};


template<typename ptr_t>

class IterF: public Iter<ptr_t>, public std::iterator<std::forward_iterator_tag, std::pair<ptr_t,std::size_t>>
{
public:
  using Iter<ptr_t>::Iter;
  std::pair<ptr_t, std::size_t> operator*() const
  {return {Iter<ptr_t>::m_ptr,Iter<ptr_t>::m_size};}
};

template<typename ptr_t>
class IterR: public Iter<ptr_t>, public std::iterator<std::forward_iterator_tag, ptr_t>
{
public:
  using Iter<ptr_t>::Iter;
  IterR(ptr_t a_ptr, int a_stride)
  { Iter<ptr_t>::m_ptr=a_ptr; Iter<ptr_t>::m_stride=a_stride;}
  ptr_t operator*() const
  {return Iter<ptr_t>::m_ptr ;}
};

template<typename Irreg, typename value_t>
bool check(Irreg start, Irreg end, value_t value);

template<typename Reg, typename value_t>
bool check(Reg start, Reg end, std::size_t count, value_t value);

template<typename Reg, typename value_t>
void set(Reg start, Reg end, std::size_t count, value_t value);

void reset(patch_t& patch, lli value);

lli sum(patch_t& patch);

typedef std::array<int,2> count_t;

int main() {

  init();
  
  print_test_header();

  bool success=true;
  
  intrank_t me = rank_me();
  intrank_t n =  rank_n();
  intrank_t nebrHi = (me + 1) % n;
  intrank_t nebrLo = (me + n - 1) % n;

  // Ring of ghost halos transfered between adjacent ranks using the three
  // different communication protocols
  patch_t* myPatchPtr = (patch_t*)allocate(sizeof(patch_t));
  dist_object<global_ptr<lli> > mesh(global_ptr<lli>((lli*)myPatchPtr));
  dist_object<count_t>  counters(count_t({{0,0}}));

  count_t& mycount = *counters;// read from me, write to me
  lli*  myPtr = (*mesh).local();
  patch_t& myPatch = *myPatchPtr;

  future<global_ptr<lli>> fneighbor_hi = mesh.fetch(nebrHi);
  future<global_ptr<lli>> fneighbor_lo = mesh.fetch(nebrLo);

  reset(myPatch, me);
  


  when_all(fneighbor_hi, fneighbor_lo).wait();
 
  global_ptr<lli> hi=fneighbor_hi.result();
  global_ptr<lli> lo=fneighbor_lo.result();

  // irregular test 1
  {
    lli srcTest[]= {me, me+1, me+2, me+3, me+4, me+5};
    std::vector<std::pair<lli*,std::size_t> > svec(1,{srcTest, 6});
    std::vector<std::pair<global_ptr<lli>, std::size_t> > dvec(1, {hi, 6});
    std::cout<<"\nsending to "<<hi.where()<<": ";
    for(int i=0; i<6; i++)
      std::cout<<" "<<srcTest[i];
    std::cout<<"\n";
    auto f0 = rput_irregular(svec.begin(), svec.end(),
                             dvec.begin(), dvec.end(),
                             remote_cx::as_rpc([](dist_object<count_t>& c) {
                                 (*c)[1]++;}, counters) |
                             operation_cx::as_future()
                             );

    mycount[0]++;  //reading from myself
    f0.wait();
    while(mycount[1]!=1)  progress(); // wait to see my lo neighbor has modified my count
 
    for(int i=0; i<6; i++)
      {
        if(myPtr[i] != nebrLo+i){
          std::cout<<" simple sequence Irregular expected "<< nebrLo+i<<" but got "<<myPtr[i]<<"\n";
          success=false;
        }
      }
  }
  // irregular put test 2
  std::cout<<"\nIrregular test 2 \n";
  auto fs1 = IterF<lli*>(myPtr+N-B, B, N);
  auto fs1_end = fs1; fs1_end+=M*N;
  auto fd1 = IterF<global_ptr<lli>>(hi, B, N);
  auto fd1_end = fd1; fd1_end+=M*N;
  auto fr1 = IterF<lli*>(myPtr, B, N);
  auto fr1_end = fr1; fr1_end+=M*N; 

  barrier(); // need to see the reset done above
  auto f1 = rput_irregular(fs1, fs1_end, fd1, fd1_end);

 
  f1.wait();
  barrier();
  
  success = success && check(fr1, fr1_end, (lli)nebrLo);
  lli sm = sum(myPatch);
  lli correctAnswer = (me*(N-B)+B*nebrLo)*M;
  
  if(sm != correctAnswer)
    {
      std::cout<<" Irregular expected sum:"<<correctAnswer<<" actual sum: "<<sm<<"\n";
      success = false;
    }

  reset(myPatch, me);
  std::cout<<"\nRegular put test 1\n";
  auto rs1 = IterR<lli*>(myPtr,N);
  auto rs1_end = rs1; rs1_end+=M*N;
  auto rd1 = IterR<global_ptr<lli>>(lo+N-B,N);
  auto rd1_end = rd1; rd1_end+=M*N;
  auto rr1 = IterR<lli*>(myPtr+N-B, N);
  auto rr1_end = rr1;  rr1_end+=M*N;
  lli token=-1;
  ::set(rr1, rr1_end, B, token);
  
  barrier();  

  auto r1 = rput_regular(rs1, rs1_end, B , rd1, rd1_end, B);

 
  r1.wait();
  barrier();
  
  success = success && check(rr1, rr1_end, B, (lli)nebrHi);
  sm = sum(myPatch);
  correctAnswer = (me*(N-B)+B*nebrHi)*M;
  if(sm != correctAnswer)
    {
      std::cout<<" Regular expected sum:"<<correctAnswer<<" actual sum: "<<sm<<"\n";
      success = false;
    }
 
  
  reset(myPatch, me);
  barrier();
  
  // strided put
  auto s1 = rput_strided<2>(myPtr+N-B, {{sizeof(lli),N*sizeof(lli)}},
                            hi, {{sizeof(lli),N*sizeof(lli)}}, {{B,M}},
                            operation_cx::as_lpc(default_persona(),[&](){ mycount[0]++;}) |
                            operation_cx::as_future() |
                            remote_cx::as_rpc([](dist_object<count_t>& c){
                                (*c)[1]++;},counters));


  s1.wait();
  while(mycount[0] !=2 || mycount[1] != 2) progress();
  // mycount[1] == 2 means that the rput to me should be completed
  // seeing mycount[0] == 2 means that the operation_cx::as_lpc has fired
  
  std::cout<<"\nStrided put testing \n";
  success = success && check(fr1, fr1_end, (lli)nebrLo); // this is moving the same data as in the irregular case
  sm = sum(myPatch);
  correctAnswer = (me*(N-B)+B*nebrLo)*M;
  if(sm != correctAnswer)
    {
      std::cout<<" Stride expected sum:"<<correctAnswer<<" actual sum: "<<sm<<"\n";
      success = false;
    }

  
  //   VIS rget tests
  
      // irregular get test 1
  std::cout<<"Irregular rget test 1\n";
  lli m=me;
  lli getTest[]= {-m, -m-1, -m-2, -m-3, -m-4, -m-5};
  for(int i=0; i<6; i++)
    {
      myPtr[i]=m+i;
    }
  std::vector<std::pair<lli*,std::size_t> > dvec(1,{getTest, 6});
  std::vector<std::pair<global_ptr<lli>, std::size_t> > svec(1, {hi, 6});
  std::cout<<"\nleaving for "<<hi.where()<<": ";
  for(int i=0; i<6; i++)
    std::cout<<" "<<myPtr[i];
  std::cout<<"\n";
  barrier();
  auto fg0 = rget_irregular(svec.begin(), svec.end(), dvec.begin(), dvec.end(),
                            operation_cx::as_lpc(default_persona(),[&](){ mycount[0]++;}) |
                            operation_cx::as_future() |
                            remote_cx::as_rpc([](dist_object<count_t>& c){
                                (*c)[1]++;},counters)
                            );

  fg0.wait();
  
  for(int i=0; i<6; i++)
    {
      if(getTest[i] != nebrHi+i){
        std::cout<<" simple sequence Irregular get expected "<< nebrHi+i<<" but got "<<getTest[i]<<"\n";
        success=false;
      }
    }

  while(mycount[0] != 3 || mycount[1] != 3) progress(); // ensure that every executes lpc and rpc completions
  barrier();
 
  
  reset(myPatch, me);
  std::cout<<"Irregular rget test 2\n";
  barrier();
  
  auto fg2 = rget_irregular(fd1, fd1_end, fs1, fs1_end);

  fr1 = IterF<lli*>(myPtr+N-B, B, N);
  fr1_end = fr1; fr1_end+=M*N;
  
  fg2.wait();

  success = success && check(fr1, fr1_end, (lli)nebrHi);
  sm = sum(myPatch);
  correctAnswer = (me*(N-B)+B*nebrHi)*M;
  if(sm != correctAnswer)
    {
      std::cout<<" Irregular get expected sum:"<<correctAnswer<<" actual sum: "<<sm<<"\n";
      success = false;
    }

  barrier();
  reset(myPatch, me);
  rr1 = IterR<lli*>(myPtr, N);
  rr1_end = rr1; rr1_end+=M*N;
  ::set(rr1, rr1_end, B, token);
  barrier();
  auto g1 = rget_regular(rd1, rd1_end, B, rs1, rs1_end, B);
  g1.wait();
  sm=sum(myPatch);
  success = success && check(rr1, rr1_end, B, (lli)nebrLo);
  correctAnswer = (me*(N-B)+B*nebrLo)*M;
  if(sm != correctAnswer)
    {
      std::cout<<" Regular get expected sum:"<<correctAnswer<<" actual sum: "<<sm<<"\n";
      success = false;
    }

  barrier();
  std::cout<<"rget_strided test 1\n";
  reset(myPatch, me);

  barrier();
  //  a bit fancier than the rput,  this one gets every other row in the source patch buffer
  //  and inserts it with a one row offset into myPatch
  auto sg1 = rget_strided<2>(lo+N-B, {{sizeof(lli), 2*N*sizeof(lli)}},
                             myPtr+N, {{sizeof(lli), 2*N*sizeof(lli)}}, {{B,M/2}});
 
  sg1.wait();
  sm=sum(myPatch);
  correctAnswer = B*M/2*me + B*M/2*nebrLo + (N-B)*M*me;

  if(sm != correctAnswer)
    {
      std::cout<<" Strided get expected sum:"<<correctAnswer<<" actual sum: "<<sm<<"\n";
      success = false;
    }
  
  print_test_success(success);
  
  finalize();
  
  return 0;
}

// support functions

void reset(patch_t& patch, lli value)
{
  for(int j=0; j<M; j++)
    {
      for(int i=0; i<N; i++)
        {
          patch[j][i]=  value;
        }
    }
}

lli sum(patch_t& patch)
{
  lli rtn=0;
  for(int j=0; j<M; j++)
    {
      lli jsum=0;
      for(int i=0; i<N; i++)
        {
          jsum+=patch[j][i];
        }
      rtn+=jsum;
    }
  return rtn;
}


template<typename Irreg, typename value_t>
bool check(Irreg start, Irreg end, value_t value)
{
  while(!(start == end))
    {
      value_t* v = std::get<0>(*start);
      value_t* e = v + std::get<1>(*start);
      for(;v<e; ++v)
        {
          if(*v != value)
            {
              std::cout<<"Frag expected value:"<<value<<" seeing:"<<*v;
              return false;
            }
        }
      ++start;
    }
  return true;
}


template<typename Reg, typename value_t>
bool check(Reg start, Reg end, std::size_t count, value_t value)
{
  while(!(start == end))
    {
      value_t* v = *start;
      value_t* e = v+count;
      for(;v!=e;++v)
        {
          if(*v != value)
            {
              std::cout<<"Regular expected value:"<<value<<" seeing:"<<*v;
              return false;
            }
        }
      ++start;
    }
  return true;
}

template<typename Reg, typename value_t>
void set(Reg start, Reg end, std::size_t count, value_t value)
{
  while(!(start == end))
    {
      value_t* v = *start;
      value_t* e = v+count;
      for(;v!=e;++v)
        {
          *v=value;
        }
      ++start;
    }
}
        
