#
# Compiler-specific settings
#
# The following are available (among others)
#   GASNET_{CC,CXX}
#   GASNET_{CC,CXX}_FAMILY
#   GASNET_{C,CXX}FLAGS
#
# NOTE: sufficiently old GNU Make lacks 'else if*'

#
# LIBUPCXX_STDCXX
# This is the C++ language level option appropriate to the compiler
#
# First the default:
LIBUPCXX_STDCXX := -std=c++11
# Then compiler-specific overrides:
ifeq ($(GASNET_CXX_FAMILY),Intel)
LIBUPCXX_STDCXX := -std=c++14
endif
# Then throw it all away if $CXX already specifies a language level:
ifneq ($(findstring -std=c++,$(GASNET_CXX)),)
LIBUPCXX_STDCXX :=
endif

#
# LIBUPCXX_CFLAGS
# Any CFLAGS specific to compilation of objects in libupcxx
#
LIBUPCXX_CFLAGS := 

#
# LIBUPCXX_CXXFLAGS
# Any CXXFLAGS specific to compilation of objects in libupcxx
#
LIBUPCXX_CXXFLAGS := 
