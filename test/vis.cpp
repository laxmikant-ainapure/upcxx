#include <cstdint>
#include <vector>
#include <cstdlib>
#include <cstddef>

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
  Iter& operator++(){m_ptr+=m_stride; return *this;}
  Iter  operator++(int){Iter rtn(*this); m_ptr+=m_stride; return rtn;} 

  void operator+=(std::ptrdiff_t a_skip) {m_ptr = m_ptr+a_skip;}

protected:
  ptr_t m_ptr=0;
  std::size_t m_size=0;
  int   m_stride=0;
};

template<typename ptr_t>
class IterF: public Iter<ptr_t>
{
public:
  using Iter<ptr_t>::Iter;
  std::pair<ptr_t, std::size_t> operator*() const
  {return {Iter<ptr_t>::m_ptr,Iter<ptr_t>::m_size};}
};

template<typename ptr_t>
class IterR: public Iter<ptr_t>
{
public:
  using Iter<ptr_t>::Iter;
  IterR(ptr_t a_ptr, int a_stride)
  { Iter<ptr_t>::m_ptr=a_ptr; Iter<ptr_t>::m_stride=a_stride;}
  
  ptr_t operator*() const
  {return Iter<ptr_t>::m_ptr ;}
};

template<typename Frag, typename value_t>
bool check(Frag start, Frag end, value_t value);

template<typename Reg, typename value_t>
bool check(Reg start, Reg end, std::size_t count, value_t value);

template<typename Reg, typename value_t>
void set(Reg start, Reg end, std::size_t count, value_t value);

void reset(patch_t& patch, lli value);

lli sum(patch_t& patch);

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

  lli*  myPtr = (*mesh).local();
  patch_t& myPatch = *myPatchPtr;

  future<global_ptr<lli>> fneighbor_hi = mesh.fetch(nebrHi);
  future<global_ptr<lli>> fneighbor_lo = mesh.fetch(nebrLo);

  reset(myPatch, me);
  


  when_all(fneighbor_hi, fneighbor_lo).wait();
 
  global_ptr<lli> hi=fneighbor_hi.result();
  global_ptr<lli> lo=fneighbor_lo.result();

  // fragmented test 1
  lli srcTest[]= {me, me+1, me+2, me+3, me+4, me+5};
  std::vector<std::pair<lli*,std::size_t> > svec(1,{srcTest, 6});
  std::vector<std::pair<global_ptr<lli>, std::size_t> > dvec(1, {hi, 6});
  std::cout<<"\nsending to "<<hi.where()<<": ";
  for(int i=0; i<6; i++)
    std::cout<<" "<<srcTest[i];
  std::cout<<"\n";
  auto f0 = rput_fragmented(svec.begin(), svec.end(), dvec.begin(), dvec.end());

  f0.wait();
  barrier();
  
  for(int i=0; i<6; i++)
    {
      if(myPtr[i] != nebrLo+i){
        std::cout<<" simple sequence Fragmented expected "<< nebrLo+i<<" but got "<<myPtr[i]<<"\n";
        success=false;
      }
    }
  // fragmented test 2
  std::cout<<"\nFragmented test 2 \n";
  auto fs1 = IterF<lli*>(myPtr+N-B, B, N);
  auto fs1_end = fs1; fs1_end+=M*N;
  auto fd1 = IterF<global_ptr<lli>>(hi, B, N);
  auto fd1_end = fd1; fd1_end+=M*N;
  auto fr1 = IterF<lli*>(myPtr, B, N);
  auto fr1_end = fr1; fr1_end+=M*N; 


  auto f1 = rput_fragmented(fs1, fs1_end, fd1, fd1_end);

 
  f1.wait();
  barrier();
  
  success = success && check(fr1, fr1_end, (lli)nebrLo);
  lli sm = sum(myPatch);
  lli correctAnswer = (me*(N-B)+B*nebrLo)*M;
  
  if(sm != correctAnswer)
    {
      std::cout<<" Fragmented expected sum:"<<correctAnswer<<" actual sum: "<<sm<<"\n";
      success = false;
    }
  
  reset(myPatch, me);
  
  // regular
  std::cout<<"\nRegular test 1\n";
  auto rs1 = IterR<lli*>(myPtr,N);
  auto rs1_end = rs1; rs1_end+=M*N;
  auto rd1 = IterR<global_ptr<lli>>(lo+N-B,N);
  auto rd1_end = rd1; rd1_end+=M*N;
  auto rr1 = IterR<lli*>(myPtr+N-B, N);
  auto rr1_end = rr1;  rr1_end+=M*N;

  lli token=-1;
  set(rr1, rr1_end, B, token);
  
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
  
  // strided
  auto s1 = rput_strided<2>(myPtr+N-B, {sizeof(lli),N*sizeof(lli)},
                            hi, {sizeof(lli),N*sizeof(lli)}, {B,M});


  s1.wait();
  barrier();
  
  std::cout<<"\nStrided testing \n";
  success = success && check(fr1, fr1_end, (lli)nebrLo); // this is moving the same data as in the fragmented case
  sm = sum(myPatch);
  correctAnswer = (me*(N-B)+B*nebrLo)*M;
  if(sm != correctAnswer)
    {
      std::cout<<" Stride expected sum:"<<correctAnswer<<" actual sum: "<<sm<<"\n";
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


template<typename Frag, typename value_t>
bool check(Frag start, Frag end, value_t value)
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
        
