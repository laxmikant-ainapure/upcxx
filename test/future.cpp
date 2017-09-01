#include <cstdint>
#include <queue>

#include <upcxx/diagnostic.hpp>
#include <upcxx/future.hpp>

#include <cstdlib>

using namespace upcxx;

struct the_q_compare {
  // shuffle bits of a pointer
  template<typename T>
  static std::uintptr_t mix(T *p) {
    std::uintptr_t u = reinterpret_cast<std::uintptr_t>(p);
    std::uintptr_t knuth = 0x9e3779b97f4a7c15u;
    u ^= u >> 35;
    u *= knuth;
    u ^= u >> 21;
    u *= knuth;
    return u;
  }
  
  template<typename T>
  bool operator()(T *a, T *b)  {
    return mix(a) < mix(b);
  }
};

// A global list of promises in a randomized order. Progress is made by
// satisfying these promises. Randomization is to immitate the wacky
// nature of asynchronous execution.
std::priority_queue<
    promise<int>*,
    std::vector<promise<int>*>,
    the_q_compare
  > the_q;

int fib_smart(int i) {
  int a = 0, b = 1;
  while(i--) {
    int c = a + b;
    a = b;
    b = c;
  }
  return a;
}

future<int> fib(int i) {
  // Instead of returning the result values, we always return futures
  // derived from partially satisfied promises containing the result value.
  // The promises are insterted into the random queue. This randomizes
  // the order the fibonacci tree gets evaluated.
  
  if(i <= 1) {
    auto *p = new promise<int>;
    p->require_anonymous(1);
    p->fulfill_result(i);
    the_q.push(p);
    return p->get_future();
  }
  else
    return when_all(fib(i-1), fib(i-2))
      >> [=](int x1, int x2)->future<int> {
        static int iter = 0;
        
        // branches are equivalent, they just test more of future's
        // internal codepaths.
        if(iter++ & 1) {
          auto *p = new promise<int>;
          p->require_anonymous(1);
          UPCXX_ASSERT_ALWAYS(x1 + x2 == fib_smart(i), "i="<<i<<" x1="<<x1<<" x2="<<x2);
          p->fulfill_result(x1 + x2);
          the_q.push(p);
          return p->get_future();
        }
        else {
          return make_future(x1 + x2);
        }
      };
}

#if 0
void* operator new(std::size_t size) {
  return std::malloc(size);
}

// hopefully we see lots of "free!"'s scattered through run and not all
// clumped up at the end.
void operator delete(void *p) {
  std::cout << "free!\n";
  std::free(p);
}
#endif

int main() {
  const int arg = 5;
  
  future<int> ans0 = fib(arg);
  future<int> ans1 = ans0 >> [](int x) { return x+1; } >> fib;
  future<int> ans2 = /*future<int>*/(ans1.then_pure([](int x){return 2*x;})) >> fib;
  
  when_all(
      // stress nested concatenation
      when_all(
        when_all(),
        ans0,
        when_all(ans1),
        ans1.then_pure([](int x) { return x*x; }),
        make_future<const int&>(arg)
      ),
      make_future<std::vector<int>>({0*0, 1*1, 2*2, 3*3, 4*4})
    ).then(
      [=](int ans0, int ans1, int ans1_sqr, int arg, const std::vector<int> &some_vec) {
        std::cout << "fib("<<arg <<") = "<<ans0<<'\n';
        std::cout << "fib("<<ans0+1<<") = "<<ans1<<'\n';
        std::cout << "fib("<<ans0+1<<")**2 = "<<ans1_sqr<<'\n';
        
        for(int i=0; i < 5; i++)
          UPCXX_ASSERT_ALWAYS(some_vec[i] == i*i);
      }
    );
  
  // drain progress queue
  while(!the_q.empty()) {
    promise<int> *p = the_q.top();
    the_q.pop();
    
    p->fulfill_anonymous(1);
    delete p;
  }
  
  UPCXX_ASSERT_ALWAYS(ans2.ready());
  std::cout << "fib("<<(2*ans1.result())<<") = "<<ans2.result()<<'\n';
  
  return 0;
}
