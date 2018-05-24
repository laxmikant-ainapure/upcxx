#ifndef _572e1a8a_95cb_4968_808c_69661ac8b133
#define _572e1a8a_95cb_4968_808c_69661ac8b133

#include <new>

#include "report.hpp"

// Include this header only from the "main" translation unit since we provide
// definitions of operator new/delete.

// Run with OPNEW=1,2,3 in environment during nobs compile:
//  0 standard operator new/delete:
//    (a.k.a malloc/free). As of 2017 this is pretty good since glibc received
//    fast hread caching for small objects.
//
//  1 ltalloc:
//    Seems like a pretty good general purpose operator new/delete replacement
//    that handles lots of small objects very well. I can't imagine us being
//    able to do any better than this. But, if we do see a gap between insane
//    and ltalloc maybe we can consider providing our own.
//
//  2 "insane" allocator:
//    This allocator is not thread safe and does not support sizes > 8K, but if
//    those global program constraints are met it should be the fastest thing
//    possible. If we can't show marked improvements using this allocator over
//    other off the shelf ones, then I don't think we need to bother writing  
//    our own allocator.

#if OPNEW == 0
  // standard operator new/delete

#elif OPNEW == 1
  #include "ltalloc.h"

#elif OPNEW == 2
  #include <cstdlib> // posix_memalign

  // The insane allocator:
  namespace opnew_insane {
    struct frobj {
      frobj *next;
    };
    
    frobj *free_bin[256] = {};
    
    constexpr int bin_of_size(std::size_t size) {
      return size==0 ? 0 : (size-1)/32;
    }
    constexpr std::size_t size_of_bin(int bin) {
      return 32*(bin + 1);
    }
    
    void opnew_populate(int bin) {
      std::size_t size = size_of_bin(bin);
      
      void *p;
      if(posix_memalign(&p, 128<<10, 128<<10))
        /*ignore*/;
      
      char *blob_beg = (char*)p;
      char *blob_end = blob_beg + (128<<10);
      
      *(int*)blob_beg = bin;
      blob_beg += 64;
      
      frobj *o_prev = nullptr;
      while(blob_beg + size <= blob_end) {
        frobj *o = (frobj*)blob_beg;
        blob_beg += size;
        o->next = o_prev;
        o_prev = o;
      }
      
      free_bin[bin] = o_prev;
    }
    
    void* opnew(std::size_t size) {
      int bin = bin_of_size(size);
      
      if(free_bin[bin] == nullptr)
        opnew_populate(bin);
      
      frobj *o = free_bin[bin];
      free_bin[bin] = o->next;
      return o;
    }
    
    void opdel(void *p) {
      frobj *o = (frobj*)p;
      int bin = *reinterpret_cast<int*>(reinterpret_cast<std::uintptr_t>(p) & -(128<<10));
      o->next = free_bin[bin];
      free_bin[bin] = o;
    }
  }

  void* operator new(std::size_t size) {
    return opnew_insane::opnew(size);
  }

  void operator delete(void *p) {
    opnew_insane::opdel(p);
  }
#endif

constexpr bench::row<const char*> opnew_row() {
  return bench::column("alloc",
    OPNEW == 0 ? "std" :
    OPNEW == 1 ? "ltalloc" :
    OPNEW == 2 ? "insane" :
    "?"
  );
}

#endif
