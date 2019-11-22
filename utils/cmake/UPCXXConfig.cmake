#[=======================================================================[.rst:
UPCXXConfig
-------

Configuration for UPC++.

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

option(UPCXX_VERBOSE "Verbose UPC++ detection" OFF)
function(UPCXX_VERB MESSAGE)
  if (UPCXX_VERBOSE OR DEFINED ENV{UPCXX_VERBOSE} )
    message(STATUS "${MESSAGE}")
  endif()
endfunction()


# Set up some auxillary vars if hints have been set
if(DEFINED ENV{UPCXX_INSTALL} )
  find_program( UPCXX_META_EXECUTABLE upcxx-meta HINTS "$ENV{UPCXX_INSTALL}/bin" NO_DEFAULT_PATH )
else()
  find_program( UPCXX_META_EXECUTABLE upcxx-meta )
endif()


if( UPCXX_META_EXECUTABLE )
  message(STATUS "Found UPCXX: ${UPCXX_META_EXECUTABLE}")

  if ((NOT DEFINED ENV{UPCXX_CODEMODE}) OR ("$ENV{UPCXX_CODEMODE}" STREQUAL "unset"))
    string(TOUPPER "${CMAKE_BUILD_TYPE}" uc_CMAKE_BUILD_TYPE)
    if (uc_CMAKE_BUILD_TYPE STREQUAL "DEBUG")
      set(ENV{UPCXX_CODEMODE} "debug" )
    else()
      set(ENV{UPCXX_CODEMODE} "O3" )
    endif()
    unset(uc_CMAKE_BUILD_TYPE)
  endif()
  if (NOT DEFINED ENV{UPCXX_THREADMODE} )
    set(ENV{UPCXX_THREADMODE} "seq" )
  endif()

  execute_process( COMMAND ${UPCXX_META_EXECUTABLE} CXXFLAGS OUTPUT_VARIABLE UPCXX_CXXFLAGS
                   OUTPUT_STRIP_TRAILING_WHITESPACE)
  execute_process( COMMAND ${UPCXX_META_EXECUTABLE} CPPFLAGS OUTPUT_VARIABLE UPCXX_CPPFLAGS
                   OUTPUT_STRIP_TRAILING_WHITESPACE)
  execute_process( COMMAND ${UPCXX_META_EXECUTABLE} LIBS OUTPUT_VARIABLE UPCXX_LIBFLAGS
                   OUTPUT_STRIP_TRAILING_WHITESPACE)
  execute_process( COMMAND ${UPCXX_META_EXECUTABLE} LDFLAGS OUTPUT_VARIABLE UPCXX_LDFLAGS
                   OUTPUT_STRIP_TRAILING_WHITESPACE)
  execute_process( COMMAND ${UPCXX_META_EXECUTABLE} CXX OUTPUT_VARIABLE UPCXX_CXX_COMPILER
                   OUTPUT_STRIP_TRAILING_WHITESPACE)
  execute_process( COMMAND ${UPCXX_META_EXECUTABLE} GASNET_CONDUIT OUTPUT_VARIABLE UPCXX_NETWORK
                   OUTPUT_STRIP_TRAILING_WHITESPACE)

  string(STRIP ${UPCXX_LIBFLAGS} UPCXX_LIBFLAGS)
  string(STRIP ${UPCXX_CPPFLAGS} UPCXX_CPPFLAGS)
  string(STRIP ${UPCXX_CXXFLAGS} UPCXX_CXXFLAGS)
  string(STRIP ${UPCXX_LDFLAGS} UPCXX_LDFLAGS)
  string(STRIP ${UPCXX_CXX_COMPILER} UPCXX_CXX_COMPILER)

  list( APPEND UPCXX_LIBRARIES ${UPCXX_LIBFLAGS})

  # move any embedded options from UPCXX_CXX_COMPILER to UPCXX_CXXFLAGS
  if (UPCXX_CXX_COMPILER MATCHES "[ \t\n]+(.+)$")
     UPCXX_VERB("embedded CXX options: ${CMAKE_MATCH_0}")
     string(REGEX REPLACE "[ \t\n]+.+$" "" UPCXX_CXX_COMPILER ${UPCXX_CXX_COMPILER})
     set(UPCXX_CXXFLAGS "${CMAKE_MATCH_0} ${UPCXX_CXXFLAGS}")
     string(STRIP ${UPCXX_CXXFLAGS} UPCXX_CXXFLAGS)
  endif()
  UPCXX_VERB("UPCXX_CXX_COMPILER=${UPCXX_CXX_COMPILER}")
  UPCXX_VERB("CMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}")
  #get absolute path, resolving symbolic links, of UPCXX_CXX_COMPILER
  find_program( ABS_UPCXX_CXX_PATH ${UPCXX_CXX_COMPILER} )
  get_filename_component(ABS_UPCXX_CXX_PATH ${ABS_UPCXX_CXX_PATH} REALPATH /)
  if (NOT EXISTS "${ABS_UPCXX_CXX_PATH}")
    message(WARNING "CANNOT FIND ABSOLUTE PATH TO UPCXX_CXX_COMPILER (${UPCXX_CXX_COMPILER})")
    set(ABS_UPCXX_CXX_PATH "${UPCXX_CXX_COMPILER}")
  endif()

  #get absolute path, resolving symbolic links, of CMAKE_CXX_COMPILER
  get_filename_component(ABS_CMAKE_CXX_PATH ${CMAKE_CXX_COMPILER} REALPATH /)
  UPCXX_VERB("ABS_UPCXX_CXX_PATH=${ABS_UPCXX_CXX_PATH}")
  UPCXX_VERB("ABS_CMAKE_CXX_PATH=${ABS_CMAKE_CXX_PATH}")

  set( UPCXX_COMPATIBLE_COMPILER FALSE)
  if("${ABS_UPCXX_CXX_PATH}" STREQUAL "${ABS_CMAKE_CXX_PATH}")
    set( UPCXX_COMPATIBLE_COMPILER TRUE)
  else()
    get_filename_component(UPCXX_CXX_NAME ${ABS_UPCXX_CXX_PATH} NAME)
    get_filename_component(CMAKE_CXX_NAME ${ABS_CMAKE_CXX_PATH} NAME)
    message(STATUS "${UPCXX_CXX_NAME} vs ${CMAKE_CXX_NAME}")
    if("${UPCXX_CXX_NAME}" STREQUAL "${CMAKE_CXX_NAME}")
      #compare the versions
      execute_process( COMMAND ${UPCXX_CXX_COMPILER}  --version OUTPUT_VARIABLE UPCXX_CXX_COMPILER_VERSION)
      string(REPLACE "\n" " " UPCXX_CXX_COMPILER_VERSION ${UPCXX_CXX_COMPILER_VERSION})
      execute_process( COMMAND ${CMAKE_CXX_COMPILER}  --version OUTPUT_VARIABLE LOC_CMAKE_CXX_COMPILER_VERSION)
      string(REPLACE "\n" " " LOC_CMAKE_CXX_COMPILER_VERSION ${LOC_CMAKE_CXX_COMPILER_VERSION})
      #message(STATUS "${UPCXX_CXX_COMPILER_VERSION} vs ${LOC_CMAKE_CXX_COMPILER_VERSION}")
      if("${UPCXX_CXX_COMPILER_VERSION}" STREQUAL "${LOC_CMAKE_CXX_COMPILER_VERSION}")
        set( UPCXX_COMPATIBLE_COMPILER TRUE)
      endif()
    endif()
  endif()

  if( NOT UPCXX_COMPATIBLE_COMPILER )
    message(WARNING "UPCXX CXX compiler provided by upcxx-meta (${UPCXX_CXX_COMPILER} -- ${ABS_UPCXX_CXX_PATH}) is different from CMAKE_CXX_COMPILER (${CMAKE_CXX_COMPILER} -- ${ABS_CMAKE_CXX_PATH})")
    message(WARNING "UPCXX cannot be used.")
  endif()

  unset(ABS_UPCXX_CXX_PATH)
  unset(ABS_CMAKE_CXX_PATH)
  unset(UPCXX_CXX_NAME)
  unset(CMAKE_CXX_NAME)
  unset(UPCXX_CXX_COMPILER_VERSION)

  #now separate include dirs from flags
  if(UPCXX_CPPFLAGS)
    string(REGEX REPLACE "[ ]+" ";" UPCXX_CPPFLAGS ${UPCXX_CPPFLAGS})
    foreach( option ${UPCXX_CPPFLAGS} )
      string(STRIP ${option} option)
      string(REGEX MATCH "^-I" UPCXX_INCLUDE ${option})
      if( UPCXX_INCLUDE )
        string( REGEX REPLACE "^-I" "" option ${option} )
        list( APPEND UPCXX_INCLUDE_DIRS ${option})
      else()
        string(REGEX MATCH "^-D" UPCXX_DEFINE ${option})
        if( UPCXX_DEFINE )
          string( REGEX REPLACE "^-D" "" option ${option} )
          list( APPEND UPCXX_DEFINITIONS ${option})
        else()
          list( APPEND UPCXX_OPTIONS ${option})
        endif()
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

foreach( dir ${UPCXX_INCLUDE_DIRS} )
  if( EXISTS ${dir}/upcxx/upcxx.hpp )
    set( version_pattern 
      "#[\t ]*define[\t ]+UPCXX_VERSION[\t ]+([0-9]+)"
      )
    #message(STATUS "checking ${dir}/upcxx/upcxx.hpp for ${version_pattern}" )
    file( STRINGS ${dir}/upcxx/upcxx.hpp upcxx_version
      REGEX ${version_pattern} )
    #message(STATUS "upcxx_version ${upcxx_version}" )

    if( ${upcxx_version} MATCHES ${version_pattern} )
      set(UPCXX_VERSION_STRING ${CMAKE_MATCH_1})
    endif()

    unset( upcxx_version )
    unset( version_pattern )
  endif()
endforeach()

if(UPCXX_VERSION_STRING)
  message( STATUS "UPCXX VERSION: " ${UPCXX_VERSION_STRING} )
endif()

# Determine if we've found UPCXX
mark_as_advanced( UPCXX_FOUND UPCXX_META_EXECUTABLE UPCXX_INCLUDE_DIRS
                  UPCXX_LIBRARIES UPCXX_DEFINITIONS UPCXX_CXX_STANDARD
                  UPCXX_OPTIONS UPCXX_LINK_OPTIONS UPCXX_COMPATIBLE_COMPILER)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args( UPCXX
  REQUIRED_VARS UPCXX_META_EXECUTABLE UPCXX_LIBRARIES UPCXX_INCLUDE_DIRS
                UPCXX_DEFINITIONS UPCXX_LINK_OPTIONS
                UPCXX_COMPATIBLE_COMPILER
  VERSION_VAR UPCXX_VERSION_STRING
  HANDLE_COMPONENTS)

message(STATUS "UPC++ requires the c++${UPCXX_CXX_STANDARD} standard.")
message(STATUS "UPCXX_NETWORK=${UPCXX_NETWORK}")
message(STATUS "UPCXX_THREADMODE=$ENV{UPCXX_THREADMODE}")
message(STATUS "UPCXX_CODEMODE=$ENV{UPCXX_CODEMODE}")

# Export a UPCXX::upcxx target for modern cmake projects
if( UPCXX_FOUND AND NOT TARGET UPCXX::upcxx )
  UPCXX_VERB( "UPCXX_INCLUDE_DIRS: ${UPCXX_INCLUDE_DIRS}" )
  UPCXX_VERB( "UPCXX_DEFINITIONS:  ${UPCXX_DEFINITIONS}" )
  UPCXX_VERB( "UPCXX_OPTIONS:      ${UPCXX_OPTIONS}" )
  UPCXX_VERB( "UPCXX_LINK_OPTIONS: ${UPCXX_LINK_OPTIONS}" )
  UPCXX_VERB( "UPCXX_LIBRARIES:    ${UPCXX_LIBRARIES}" )
  add_library( UPCXX::upcxx INTERFACE IMPORTED )
  set_target_properties( UPCXX::upcxx PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${UPCXX_INCLUDE_DIRS}"
    INTERFACE_LINK_LIBRARIES      "${UPCXX_LIBRARIES}"
    INTERFACE_LINK_OPTIONS        "${UPCXX_LINK_OPTIONS}"
    INTERFACE_COMPILE_DEFINITIONS "${UPCXX_DEFINITIONS}"
    INTERFACE_COMPILE_OPTIONS     "${UPCXX_OPTIONS}"
    INTERFACE_COMPILE_FEATURES    "cxx_std_${UPCXX_CXX_STANDARD}"
    )
  set(UPCXX_LIBRARIES UPCXX::upcxx)
endif()
