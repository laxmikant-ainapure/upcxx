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
# UPCXX_STDCXX
# This is the C++ language level option appropriate to the compiler
#
# First the default:
UPCXX_STDCXX := -std=c++11
# Then compiler-specific overrides:
ifeq ($(GASNET_CXX_FAMILY),Intel)
UPCXX_STDCXX := -std=c++14
endif
# Then throw it all away if $CXX already specifies a language level:
ifneq ($(findstring -std=c++,$(GASNET_CXX))$(findstring -std=gnu++,$(GASNET_CXX)),)
UPCXX_STDCXX :=
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

# Address issue #286 with PGI compiler
ifeq ($(GASNET_CXX_FAMILY),PGI)
LIBUPCXX_CXXFLAGS := --diag_suppress1427
endif

# Address issue #286 with Intel compiler (for -Wextra only)
ifeq ($(GASNET_CXX_FAMILY),Intel)
ifneq ($(findstring -Wextra,$(GASNET_CXX) $(GASNET_CXXFLAGS)),)
LIBUPCXX_CXXFLAGS := -Wno-invalid-offsetof
endif
endif

#
# UPCXX_DEP_FLAGS
# Incantation to generate compiler flags for creating a dependency file on stdout.
# The resulting output should name both arguments as "targets", dependent on all
# files visited in preprocess.  This ensures both the object (or executable) and
# dependency file are kept up-to-date.
#
# Example:
#   $(CXX) $(call UPCXX_DEP_FLAGS,$(target).o,$(target).d) $(src_file) [more flags]
#
# Note 1: Generation to stdout is used because PGI compilers ignore `-o foo` in
# the presence `-E` and lack support for `-MF`.  Meanwhile all supported
# compilers send `-E` output to stdout by default.
#
# TODO: Options like `-MM` can save time (omitting numerous system headers from
# the list of files to stat when building).  However, it is not currently used
# because `nobs` documents the behavior of `-MM` as broken with PGI.
# Compiler-family could be used to include/exclude such flags here.
#
# Some explanation of the default flags, extracted from gcc man page:
#   -M
#       Instead of outputting the result of preprocessing, output a rule
#       suitable for make describing the dependencies [...]
#   -MT target
#       Change the target of the rule emitted by dependency generation. [...]
UPCXX_DEP_FLAGS = -E -M -MT '$(1) $(2)'
