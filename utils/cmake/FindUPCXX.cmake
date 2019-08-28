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


if( DEFINED ENV{UPCXX_INSTALL} )
  find_program( UPCXX_EXECUTABLE upcxx-meta HINTS $ENV{UPCXX_INSTALL}/bin NO_DEFAULT_PATH )
else()
  find_program( UPCXX_EXECUTABLE upcxx-meta )
endif()


if( UPCXX_EXECUTABLE )
  execute_process( COMMAND ${UPCXX_EXECUTABLE} CXXFLAGS OUTPUT_VARIABLE UPCXX_CXXFLAGS)
  execute_process( COMMAND ${UPCXX_EXECUTABLE} CPPFLAGS OUTPUT_VARIABLE UPCXX_PPFLAGS)
  execute_process( COMMAND ${UPCXX_EXECUTABLE} LIBS OUTPUT_VARIABLE UPCXX_LIBFLAGS)

  string(REPLACE "\n" " " UPCXX_LIBFLAGS ${UPCXX_LIBFLAGS})
  string(REPLACE "\n" " " UPCXX_PPFLAGS ${UPCXX_PPFLAGS})
  string(REPLACE "\n" " " UPCXX_CXXFLAGS ${UPCXX_CXXFLAGS})

  string(STRIP ${UPCXX_LIBFLAGS} UPCXX_LIBFLAGS)
  string(STRIP ${UPCXX_PPFLAGS}  UPCXX_PPFLAGS)
  string(STRIP ${UPCXX_CXXFLAGS}  UPCXX_CXXFLAGS)
  
  list( APPEND UPCXX_LIBRARIES ${UPCXX_LIBFLAGS})

  #now separate include dirs from flags
  if(UPCXX_PPFLAGS)
    string(REGEX REPLACE "[ ]+" ";" UPCXX_PPFLAGS ${UPCXX_PPFLAGS})
    foreach( option ${UPCXX_PPFLAGS} )
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
      endif()
    endforeach()
  endif()

  #extract the required cxx standard from the flags
  if(UPCXX_CXXFLAGS)
    string(REGEX REPLACE "[ ]+" ";" UPCXX_CXXFLAGS ${UPCXX_CXXFLAGS})
    foreach( option ${UPCXX_CXXFLAGS} )
      string(REGEX MATCH "^-std=" tmp_option ${option})
      if( tmp_option )
        string( REGEX REPLACE "^-std=(.+)\\+\\+" "" UPCXX_CXX_STANDARD ${option} )
        if (CMAKE_MATCH_1)
          set(UPCXX_CXX_STANDARD_TYPE ${CMAKE_MATCH_1})
        endif()
      endif()
    endforeach()
  endif()

  unset( UPCXX_CXXFLAGS )
  unset( UPCXX_LIBFLAGS )
  unset( UPCXX_PPFLAGS )
  unset( UPCXX_INCLUDE )
  unset( UPCXX_DEFINE )
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
mark_as_advanced( UPCXX_FOUND UPCXX_EXECUTABLE UPCXX_INCLUDE_DIRS UPCXX_LIBRARIES UPCXX_DEFINITIONS UPCXX_CXX_STANDARD)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args( UPCXX
  REQUIRED_VARS UPCXX_EXECUTABLE UPCXX_LIBRARIES UPCXX_INCLUDE_DIRS UPCXX_DEFINITIONS 
  VERSION_VAR UPCXX_VERSION_STRING
  HANDLE_COMPONENTS
)

message(STATUS "UPC++ requires the ${UPCXX_CXX_STANDARD_TYPE}++${UPCXX_CXX_STANDARD} standard.")


# Export a UPCXX::upcxx target for modern cmake projects
if( UPCXX_FOUND AND NOT TARGET UPCXX::upcxx )
  add_library( UPCXX::upcxx INTERFACE IMPORTED )
  set_target_properties( UPCXX::upcxx PROPERTIES
    INTERFACE_INCLUDE_DIRSECTORIES "${UPCXX_INCLUDE_DIRS}"
    INTERFACE_LINK_LIBRARIES      "${UPCXX_LIBRARIES}" 
    INTERFACE_COMPILE_DEFINITIONS "${UPCXX_DEFINITIONS}" 
    INTERFACE_COMPILE_FEATURES    "cxx_${UPCXX_CXX_STANDARD_TYPE}_${UPCXX_CXX_STANDARD}"
  )
endif()
