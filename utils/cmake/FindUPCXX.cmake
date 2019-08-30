#[=======================================================================[.rst:
FindUPCXX
-------

Find a UPC++ implementation.

UPC++ is a C++ library that supports Partitioned Global Address Space
(PGAS) programming, and is designed to interoperate smoothly and
efficiently with MPI, OpenMP, CUDA and AMTs. It leverages GASNet-EX to
deliver low-overhead, fine-grained communication, including Remote Memory
Access (RMA) and Remote Procedure Call (RPC).

This module checks if either the upcxx-meta utility can be found in the path
or in the bin sub-directory located inside the path pointed by the 
``UPCXX_INSTALL`` environment variable if it is defined.

#]=======================================================================]


cmake_minimum_required( VERSION 3.11 ) # Require CMake 3.11+
# Set up some auxillary vars if hints have been set


if( UPCXX_INSTALL )
  find_program( UPCXX_META_EXECUTABLE upcxx-meta HINTS ${UPCXX_INSTALL}/bin NO_DEFAULT_PATH )
else()
  find_program( UPCXX_META_EXECUTABLE upcxx-meta )
endif()


if( UPCXX_META_EXECUTABLE )
  execute_process( COMMAND ${UPCXX_META_EXECUTABLE} CXXFLAGS OUTPUT_VARIABLE UPCXX_CXXFLAGS)
  execute_process( COMMAND ${UPCXX_META_EXECUTABLE} CPPFLAGS OUTPUT_VARIABLE UPCXX_CPPFLAGS)
  execute_process( COMMAND ${UPCXX_META_EXECUTABLE} LIBS OUTPUT_VARIABLE UPCXX_LIBFLAGS)
  execute_process( COMMAND ${UPCXX_META_EXECUTABLE} LDFLAGS OUTPUT_VARIABLE UPCXX_LDFLAGS)
  execute_process( COMMAND ${UPCXX_META_EXECUTABLE} CXX OUTPUT_VARIABLE UPCXX_CXX_COMPILER)

  string(REPLACE "\n" " " UPCXX_LIBFLAGS ${UPCXX_LIBFLAGS})
  string(REPLACE "\n" " " UPCXX_CPPFLAGS ${UPCXX_CPPFLAGS})
  string(REPLACE "\n" " " UPCXX_CXXFLAGS ${UPCXX_CXXFLAGS})
  string(REPLACE "\n" " " UPCXX_LDFLAGS ${UPCXX_LDFLAGS})
  string(REPLACE "\n" " " UPCXX_CXX_COMPILER ${UPCXX_CXX_COMPILER})

  string(STRIP ${UPCXX_LIBFLAGS} UPCXX_LIBFLAGS)
  string(STRIP ${UPCXX_CPPFLAGS} UPCXX_CPPFLAGS)
  string(STRIP ${UPCXX_CXXFLAGS} UPCXX_CXXFLAGS)
  string(STRIP ${UPCXX_LDFLAGS} UPCXX_LDFLAGS)
  string(STRIP ${UPCXX_CXX_COMPILER} UPCXX_CXX_COMPILER)
  
  list( APPEND UPCXX_LIBRARIES ${UPCXX_LIBFLAGS})

  if(NOT ("${UPCXX_CXX_COMPILER}" STREQUAL "${CMAKE_CXX_COMPILER}"))
    message(WARNING "UPCXX CXX compiler provided by upcxx-meta (${UPCXX_CXX_COMPILER}) is different from CMAKE_CXX_COMPILER (${CMAKE_CXX_COMPILER})")
    message(WARNING "Checking compilers compatibility (ABI and compile features). USE AT YOUR OWN RISK.")
    #check compiler's ABI compatibility
    include(${CMAKE_ROOT}/Modules/CMakePushCheckState.cmake)
    cmake_push_check_state(RESET)
    set( PREV_CMAKE_CXX_SIZEOF_DATA_PTR ${CMAKE_CXX_SIZEOF_DATA_PTR})
    set( PREV_CMAKE_CXX_COMPILER_ABI ${CMAKE_CXX_COMPILER_ABI} ) 
    set( PREV_CMAKE_CXX_COMPILE_FEATURES ${CMAKE_CXX_COMPILE_FEATURES} ) 
    set( PREV_CMAKE_CXX_LIBRARY_ARCHITECTURE ${CMAKE_CXX_LIBRARY_ARCHITECTURE})
    set( PREV_CMAKE_CXX_COMPILER ${CMAKE_CXX_COMPILER})
    unset(CMAKE_CXX_ABI_COMPILED)

    set(CMAKE_CXX_COMPILER ${UPCXX_CXX_COMPILER})
    
    # Try to identify the ABI and configure it into CMakeCXXCompiler.cmake
    include(${CMAKE_ROOT}/Modules/CMakeDetermineCompilerABI.cmake)
    CMAKE_DETERMINE_COMPILER_ABI(CXX ${CMAKE_ROOT}/Modules/CMakeCXXCompilerABI.cpp)


    if(NOT ("${CMAKE_CXX_SIZEOF_DATA_PTR}" STREQUAL "${PREV_CMAKE_CXX_SIZEOF_DATA_PTR}"
     AND "${CMAKE_CXX_COMPILER_ABI}" STREQUAL "${PREV_CMAKE_CXX_COMPILER_ABI}"
     AND "${CMAKE_CXX_LIBRARY_ARCHITECTURE}" STREQUAL "${PREV_CMAKE_CXX_LIBRARY_ARCHITECTURE}") ) 
     message( FATAL_ERROR "UPCXX was built with ${UPCXX_CXX_COMPILER}, while CMAKE_CXX_COMPILER is"
       " set to ${PREV_CMAKE_CXX_COMPILER}, and their ABIs are not compatible. Please use compatible/same compilers.")
    endif()


    # Try to identify the compiler features
    include(${CMAKE_ROOT}/Modules/CMakeDetermineCompileFeatures.cmake)
    CMAKE_DETERMINE_COMPILE_FEATURES(CXX)
    
    #compare the compile feature. CMAKE_CXX_COMPILER must have ALL the features that UPCXX_CXX_COMPILER has.
    set(COMPILER_FEATURE_COMPATIBLE TRUE)
    foreach( feature ${CMAKE_CXX_COMPILE_FEATURES} )
      if( NOT ( "${feature}" IN_LIST PREV_CMAKE_CXX_COMPILE_FEATURES))
        message(STATUS "Compile feature \"${feature}\" can't be found")
        set(COMPILER_FEATURE_COMPATIBLE FALSE)
        break()
      endif()
    endforeach()

    if(NOT COMPILER_FEATURE_COMPATIBLE)
      message(FATAL_ERROR "${UPCXX_CXX_COMPILER} and ${PREV_CMAKE_CXX_COMPILER} are not compile feature compatible")
    endif()

    unset(COMPILER_FEATURE_COMPATIBLE) 

    set( CMAKE_CXX_SIZEOF_DATA_PTR ${PREV_CMAKE_CXX_SIZEOF_DATA_PTR})
    set( CMAKE_CXX_COMPILER_ABI ${PREV_CMAKE_CXX_COMPILER_ABI} ) 
    set( CMAKE_CXX_COMPILE_FEATURES ${PREV_CMAKE_CXX_COMPILE_FEATURES} ) 
    set( CMAKE_CXX_LIBRARY_ARCHITECTURE ${PREV_CMAKE_CXX_LIBRARY_ARCHITECTURE})
    set( CMAKE_CXX_COMPILER ${PREV_CMAKE_CXX_COMPILER})
    cmake_pop_check_state()
  endif()

  #now separate include dirs from flags
  if(UPCXX_CPPFLAGS)
    string(REGEX REPLACE "[ ]+" ";" UPCXX_CPPFLAGS ${UPCXX_CPPFLAGS})
    foreach( option ${UPCXX_CPPFLAGS} )
      string(STRIP ${option} option)
      string(REGEX MATCH "^-I" UPCXX_INCLUDE ${option})
      if( UPCXX_INCLUDE )
        string( REGEX REPLACE "^-I" "" option ${option} )
        list( APPEND UPCXX_INCLUDE_DIRS ${option})
      endif()
      string(REGEX MATCH "^-D" UPCXX_DEFINE ${option})
      if( UPCXX_DEFINE )
        string( REGEX REPLACE "^-D" "" option ${option} )
        list( APPEND UPCXX_DEFINITIONS ${option})
      else()
        list( APPEND UPCXX_OPTIONS ${option})
      endif()
    endforeach()
  endif()

  if(UPCXX_LDFLAGS)
    string(REGEX REPLACE "[ ]+" ";" UPCXX_LDFLAGS ${UPCXX_LDFLAGS})
    foreach( option ${UPCXX_LDFLAGS} )
      string(STRIP ${option} option)
      string(REGEX MATCH "^-O" UPCXX_OPTIMIZATION ${option})
      if(NOT UPCXX_OPTIMIZATION)
        list( APPEND UPCXX_LINK_OPTIONS ${option})
      endif()
    endforeach()
  endif()

  #extract the required cxx standard from the flags
  if(UPCXX_CXXFLAGS)
    string(REGEX REPLACE "[ ]+" ";" UPCXX_CXXFLAGS ${UPCXX_CXXFLAGS})
    foreach( option ${UPCXX_CXXFLAGS} )
      string(REGEX MATCH "^-std=" tmp_option ${option})
      if( tmp_option )
        string( REGEX REPLACE "^-std=.+\\+\\+" "" UPCXX_CXX_STANDARD ${option} )
      endif()
    endforeach()
  endif()

  unset( UPCXX_CXXFLAGS )
  unset( UPCXX_LIBFLAGS )
  unset( UPCXX_CPPFLAGS )
  unset( UPCXX_LDFLAGS )
  unset( UPCXX_INCLUDE )
  unset( UPCXX_DEFINE )
  unset( UPCXX_OPTIMIZATION )
endif()




foreach( dir ${UPCXX_UPCXX_INCLUDE_DIRS} )
  if( EXISTS ${dir}/upcxx/upcxx.h )
    set( version_pattern 
      "^#define[\t ]+UPCXX_VERSION[\t ]+([0-9]+)$"
    )
    file( STRINGS ${dir}/upcxx/upcxx.h upcxx_version
          REGEX ${version_pattern} )
    
    foreach( match ${upcxx_version} )
      set(UPCXX_VERSION_STRING ${CMAKE_MATCH_2})
    endforeach()
    
    unset( upcxx_version )
    unset( version_pattern )
  endif()
endforeach()

if(UPCXX_VERSION_STRING)
  message( STATUS "UPCXX VERSION: " ${UPCXX_VERSION_STRING} )
endif()

# Determine if we've found UPCXX
mark_as_advanced( UPCXX_FOUND UPCXX_META_EXECUTABLE UPCXX_INCLUDE_DIRS UPCXX_LIBRARIES UPCXX_DEFINITIONS UPCXX_CXX_STANDARD UPCXX_OPTIONS UPCXX_LINK_OPTIONS)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args( UPCXX
  REQUIRED_VARS UPCXX_META_EXECUTABLE UPCXX_LIBRARIES UPCXX_INCLUDE_DIRS UPCXX_DEFINITIONS UPCXX_OPTIONS UPCXX_LINK_OPTIONS
  VERSION_VAR UPCXX_VERSION_STRING
  HANDLE_COMPONENTS
)

message(STATUS "UPC++ requires the c++${UPCXX_CXX_STANDARD} standard.")


# Export a UPCXX::upcxx target for modern cmake projects
if( UPCXX_FOUND AND NOT TARGET UPCXX::upcxx )
  add_library( UPCXX::upcxx INTERFACE IMPORTED )
  set_target_properties( UPCXX::upcxx PROPERTIES
    INTERFACE_INCLUDE_DIRSECTORIES "${UPCXX_INCLUDE_DIRS}"
    INTERFACE_LINK_LIBRARIES      "${UPCXX_LIBRARIES}" 
    INTERFACE_LINK_OPTIONS      "${UPCXX_LINK_OPTIONS}" 
    INTERFACE_COMPILE_DEFINITIONS "${UPCXX_DEFINITIONS}" 
    INTERFACE_COMPILE_OPTIONS "${UPCXX_OPTIONS}" 
    INTERFACE_COMPILE_FEATURES    "cxx_std_${UPCXX_CXX_STANDARD}"
  )
  set(UPCXX_LIBRARIES UPCXX::upcxx)
endif()
