#ifndef _f49d3597_3d5a_4d7a_822c_d7e602400723
#define _f49d3597_3d5a_4d7a_822c_d7e602400723

#include <upcxx/cuda.hpp>
#include <upcxx/diagnostic.hpp>

#if UPCXX_CUDA_ENABLED
  #include <cuda.h>
  #include <cuda_runtime_api.h>

  #define CU_CHECK(expr) do { \
      CUresult res_xxxxxx = (expr); \
      UPCXX_ASSERT(res_xxxxxx == CUDA_SUCCESS, "CUDA returned "<<res_xxxxxx<<": " #expr); \
    } while(0)

  #define CU_CHECK_ALWAYS(expr) do { \
      CUresult res_xxxxxx = (expr); \
      UPCXX_ASSERT_ALWAYS(res_xxxxxx == CUDA_SUCCESS, "CUDA returned "<<res_xxxxxx<<": " #expr); \
    } while(0)

  #define CURT_CHECK(expr) do { \
      cudaError_t res_xxxxxx = (expr); \
      UPCXX_ASSERT(res_xxxxxx == cudaSuccess, "CUDA returned "<<res_xxxxxx<<": " #expr); \
    } while(0)

  #define CURT_CHECK_ALWAYS(expr) do { \
      cudaError_t res_xxxxxx = (expr); \
      UPCXX_ASSERT_ALWAYS(res_xxxxxx == cudaSuccess, "CUDA returned "<<res_xxxxxx<<": " #expr); \
    } while(0)

  namespace upcxx {
    namespace cuda {
      struct device_state {
        CUcontext context;
        CUstream stream;
        CUdeviceptr segment_to_free;
      };
    }
  }
#endif
#endif
