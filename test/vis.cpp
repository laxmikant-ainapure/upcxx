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

#define M 66
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

void reset(patch_t& patch, lli value);

lli sum(patch_t& patch);

int main() {

  init();
  
  print_test_header();

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

  // fragmented
  auto fs1 = IterF<lli*>(myPtr+N-B, B, N);
  auto fs1_end = fs1; fs1_end+=M*N;
  auto fd1 = IterF<global_ptr<lli>>(hi, B, N);
  auto fd1_end = fd1; fd1_end+=M*N;
  auto fr1 = IterF<lli*>(myPtr, B, N);
  auto fr1_end = fr1; fr1+=M*N; 


  auto f1 = rput_fragmented(fs1, fs1_end, fd1, fd1_end);

  f1.wait();

  check(fr1, fr1_end, (lli)nebrLo);
  lli sm = sum(myPatch);
  lli correctAnswer = me*(N-B)*M+B*nebrLo;
  UPCXX_ASSERT_ALWAYS(sm == correctAnswer, "expected "<<correctAnswer<<", got " << sm);

  reset(myPatch, me);
  
  // regular
  auto rs1 = IterR<lli*>(myPtr,N);
  auto rs1_end = rs1; rs1_end+=M*N/2;
  auto rd1 = IterR<global_ptr<lli>>(lo+N-B,N);
  auto rd1_end = rd1; rd1_end+=M*N/2;
  auto rr1 = IterR<lli*>(myPtr+N-B, N);
  auto rr1_end = rr1;  rr1_end+=M*N;
  
  auto r1 = rput_regular(rs1, rs1_end, B , rd1, rd1_end, B);

  r1.wait();

  check(rr1, rr1_end, B, (lli)nebrHi);
  sm = sum(myPatch);
  correctAnswer = me*(N-B)+B*nebrHi;
  UPCXX_ASSERT_ALWAYS(sm == correctAnswer, "expected "<<correctAnswer<<", got " << sm);
  
  reset(myPatch, me);
  
  // strided
  auto s1 = rput_strided<2>(myPtr+N-B, {1,N}, hi, {1,N}, {B,M});

  s1.wait();

  check(fr1, fr1_end, (lli)nebrLo); // this is moving the same data as in the fragmented case
  sm = sum(myPatch);
  correctAnswer = me*(N-B)*M+B*nebrLo;
  UPCXX_ASSERT_ALWAYS(sm == correctAnswer, "expected "<<correctAnswer<<", got " << sm);
  
  //UPCXX_ASSERT_ALWAYS(ans2.ready(), "Answer is not ready");
 
  //UPCXX_ASSERT_ALWAYS(ans2.result() == 987, "expected 987, got " << ans2.result());
  
  print_test_success();

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
              std::cout<<" expected value:"<<value<<" seeing:"<<*v;
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
              std::cout<<" expected value:"<<value<<" seeing:"<<*v;
              return false;
            }
        }
      ++start;
    }
  return true;
}
        
