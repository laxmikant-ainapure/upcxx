#include <cstdint>
#include <vector>
#include <cstdlib>

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


typedef double patch_t[M][N];

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


int main() {

  init();
  
  print_test_header();

  intrank_t me = rank_me();
  intrank_t n =  rank_n();
  intrank_t nebrHi = (me + 1) % n;
  intrank_t nebrLo = (me - 1) % n;
  // Ring of ghost halos transfered between adjacent ranks using the three
  // different communication protocols
  patch_t* myPatchPtr = (patch_t*)allocate(sizeof(patch_t));
  dist_object<global_ptr<double> > mesh(global_ptr<double>((double*)myPatchPtr));

  double*  myPtr = (*mesh).local();
  patch_t& myPatch = *myPatchPtr;

  future<global_ptr<double> >fneighbor_hi = mesh.fetch(nebrHi);
  future<global_ptr<double> >fneighbor_lo = mesh.fetch(nebrLo);

  patch_t source;
  for(int i=0; i<M; i++)
    {
      for(int j=0; j<N; j++)
        {
          myPatch[i][j]= (double)me;
        }
    }
 
  wait(when_all(fneighbor_hi, fneighbor_lo));
 
  global_ptr<double> hi=fneighbor_hi.result();
  global_ptr<double> lo=fneighbor_lo.result();

  auto s1 = IterF<double*>(myPtr, 20, N);

  auto s2 = IterR<double*>(myPtr,N);
  
  //UPCXX_ASSERT_ALWAYS(ans2.ready(), "Answer is not ready");
 
  //UPCXX_ASSERT_ALWAYS(ans2.result() == 987, "expected 987, got " << ans2.result());
  
  print_test_success();

  finalize();
  
  return 0;
}
